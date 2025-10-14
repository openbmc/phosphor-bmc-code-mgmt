#include "mp2x6xx.hpp"

#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

static constexpr size_t vendorIdLength = 3;
static constexpr size_t deviceIdLength = 4;
static constexpr size_t configIdLength = 2;
static constexpr size_t statusByteLength = 1;
static constexpr size_t crcLength = 2;

static constexpr std::string_view productIdRegName = "TRIM_MFR_PRODUCT_ID2";
static constexpr std::string_view crcUserRegName = "CRC_USER";

static constexpr uint8_t pageMask = 0x0F;
static constexpr uint8_t configMask = 0xF0;

static constexpr uint8_t disableWriteProtect = 0x00;
static constexpr uint16_t disablePage2WriteProtect = 0x128C;
static constexpr uint16_t disablePage3WriteProtect = 0x0082;

enum class MP2X6XXCmd : uint8_t
{
    // Page 0 commands
    readCRCReg = 0xED,
    // Page 1 commands
    mfrMTPMemoryCtrl = 0xCC,
    // Page 2 commands
    selectConfigCtrl = 0x1A,
    // Page 3 commands
    mfrMTPMemoryCtrlPage3 = 0x81,
};

sdbusplus::async::task<bool> MP2X6XX::parseDeviceConfiguration()
{
    if (!configuration)
    {
        error("Device configuration not initialized");
        co_return false;
    }

    configuration->vendorId = 0x4D5053;

    for (const auto& tokens : parser->lineTokens)
    {
        if (!parser->isValidDataTokens(tokens))
        {
            continue;
        }

        auto regName = parser->getVal<std::string>(tokens, ATE::regName);
        if (regName == productIdRegName)
        {
            configuration->productId =
                parser->getVal<uint32_t>(tokens, ATE::regDataHex);
            configuration->configId =
                parser->getVal<uint32_t>(tokens, ATE::configId);
        }
        else if (regName == crcUserRegName)
        {
            configuration->crcUser =
                parser->getVal<uint32_t>(tokens, ATE::regDataHex);
            break;
        }
    }

    co_return true;
}

sdbusplus::async::task<bool> MP2X6XX::verifyImage(const uint8_t* image,
                                                  size_t imageSize)
{
    if (!co_await parseImage(image, imageSize, MPSImageType::type1))
    {
        error("Image verification failed: image parsing failed");
        co_return false;
    }

    if (configuration->registersData.empty())
    {
        error("Image verification failed - no data found");
        co_return false;
    }

    if (configuration->productId == 0 || configuration->configId == 0)
    {
        error("Image verification failed - missing product or config ID");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MP2X6XX::checkId(PMBusCmd pmBusCmd,
                                              uint32_t expected)
{
    const uint8_t cmd = static_cast<uint8_t>(pmBusCmd);
    size_t idLen = 0;
    bool blockRead = false;

    switch (pmBusCmd)
    {
        case PMBusCmd::mfrId:
            idLen = vendorIdLength;
            blockRead = true;
            break;
        case PMBusCmd::icDeviceId:
            idLen = deviceIdLength;
            blockRead = true;
            break;
        case PMBusCmd::mfrSerial:
            idLen = configIdLength;
            break;
        default:
            error("Invalid command for ID check: {CMD}", "CMD", lg2::hex, cmd);
            co_return false;
    }

    std::vector<uint8_t> rbuf;
    std::vector<uint8_t> tbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for ID check");
        co_return false;
    }

    tbuf = {cmd};
    rbuf.resize(idLen + (blockRead ? statusByteLength : 0));

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("I2C failure during ID check, cmd {CMD}", "CMD", lg2::hex, cmd);
        co_return false;
    }

    auto idBytes = std::span(rbuf).subspan(blockRead ? statusByteLength : 0);
    uint32_t id = bytesToInt<uint32_t>(idBytes);

    if (id != expected)
    {
        error("ID check failed for cmd {CMD}: got {ID}, expected {EXP}", "CMD",
              lg2::hex, cmd, "ID", lg2::hex, id, "EXP", lg2::hex, expected);
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MP2X6XX::unlockWriteProtect()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for unlocking write protect");
        co_return false;
    }

    tbuf = buildByteVector(PMBusCmd::writeProtect, disableWriteProtect);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to disable write protect");
        co_return false;
    }

    // unlock page 2 write protect
    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page1);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 1 for unlocking write protect for page 2");
        co_return false;
    }

    tbuf =
        buildByteVector(MP2X6XXCmd::mfrMTPMemoryCtrl, disablePage2WriteProtect);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to unlock page 2 write protect");
        co_return false;
    }

    // unlock page 3 write protect
    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page3);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 3 for unlocking write protect for page 3");
        co_return false;
    }

    tbuf = buildByteVector(MP2X6XXCmd::mfrMTPMemoryCtrlPage3,
                           disablePage3WriteProtect);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to unlock page 3 write protect");
        co_return false;
    }

    debug("Unlocked write protect");

    co_return true;
}

sdbusplus::async::task<bool> MP2X6XX::selectConfig(uint8_t config)
{
    // MPS config select command:
    // Writes to Page 2 @ 0x1A: value = 0x0F00 | ((config + 7) << 4)
    // For config 1–6 → result: 0x0F80 to 0x0FD0

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page2);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 2 for configuration switch");
        co_return false;
    }

    constexpr uint8_t baseOffset = 7;
    uint8_t encodedNibble = static_cast<uint8_t>((config + baseOffset) << 4);
    uint16_t command = 0x0F00 | encodedNibble;

    tbuf = buildByteVector(MP2X6XXCmd::selectConfigCtrl, command);

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to write config select command {CMD} for config {CONFIG}",
              "CMD", lg2::hex, command, "CONFIG", config);
        co_return false;
    }

    debug("Switched to config {CONFIG}", "CONFIG", config);
    co_return true;
}

sdbusplus::async::task<bool> MP2X6XX::programConfigData(
    const std::vector<MPSData>& gdata)
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    for (const auto& data : gdata)
    {
        uint8_t page = data.page & pageMask;

        tbuf = buildByteVector(PMBusCmd::page, page);
        if (!i2cInterface.sendReceive(tbuf, rbuf))
        {
            error("Failed to set page {PAGE} for register {REG}", "PAGE", page,
                  "REG", lg2::hex, data.addr);
            co_return false;
        }

        tbuf = {data.addr};
        tbuf.insert(tbuf.end(), data.data.begin(),
                    data.data.begin() + data.length);

        if (!i2cInterface.sendReceive(tbuf, rbuf))
        {
            error(
                "Failed to write data {DATA} to register {REG} on page {PAGE}",
                "DATA", lg2::hex, bytesToInt<uint32_t>(data.data), "REG",
                lg2::hex, data.addr, "PAGE", page);
            co_return false;
        }
    }

    if (!co_await storeUserCode())
    {
        error("Failed to store user code after programming config data");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MP2X6XX::configAllRegisters()
{
    for (const auto& [config, gdata] : getGroupedConfigData(configMask, 4))
    {
        debug("Configuring registers for config {CONFIG}", "CONFIG", config);

        // Select the appropriate config before programming its registers
        if (config > 0 && !co_await selectConfig(config))
        {
            co_return false;
        }

        if (!co_await programConfigData(gdata))
        {
            error("Failed to program configuration {CONFIG}", "CONFIG", config);
            co_return false;
        }

        debug("Configured {SIZE} registers for config {CONFIG}", "SIZE",
              gdata.size(), "CONFIG", config);
    }

    co_return true;
}

sdbusplus::async::task<bool> MP2X6XX::storeUserCode()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for storing user code");
        co_return false;
    }

    tbuf = buildByteVector(PMBusCmd::storeUserCode);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to store user code");
        co_return false;
    }

    // Wait store user code
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::milliseconds(500));

    debug("Stored user code");

    co_return true;
}

sdbusplus::async::task<bool> MP2X6XX::getCRC(uint32_t* checksum)
{
    if (checksum == nullptr)
    {
        error("getCRC() called with null checksum pointer");
        co_return false;
    }

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for CRC read");
        co_return false;
    }

    tbuf = buildByteVector(MP2X6XXCmd::readCRCReg);
    rbuf.resize(crcLength);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read CRC from device");
        co_return false;
    }

    if (rbuf.size() < crcLength)
    {
        error("CRC read returned insufficient data");
        co_return false;
    }

    *checksum = bytesToInt<uint32_t>(rbuf);

    debug("Read CRC: {CRC}", "CRC", lg2::hex, *checksum);

    co_return true;
}

sdbusplus::async::task<bool> MP2X6XX::checkMTPCRC()
{
    uint32_t crc = 0;
    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    if (!co_await getCRC(&crc))
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    {
        error("Failed to get CRC for MTP check");
        co_return false;
    }

    debug("MTP CRC: {CRC}, Expected: {EXP}", "CRC", lg2::hex, crc, "EXP",
          lg2::hex, configuration->crcUser);

    co_return configuration->crcUser == crc;
}

bool MP2X6XX::forcedUpdateAllowed()
{
    return true;
}

sdbusplus::async::task<bool> MP2X6XX::updateFirmware(bool force)
{
    (void)force;

    if (!co_await checkId(PMBusCmd::mfrId, configuration->vendorId))
    {
        co_return false;
    }

    if (!co_await checkId(PMBusCmd::icDeviceId, configuration->productId))
    {
        co_return false;
    }

    if (!co_await checkId(PMBusCmd::mfrSerial, configuration->configId))
    {
        co_return false;
    }

    if (!co_await unlockWriteProtect())
    {
        co_return false;
    }

    if (!co_await configAllRegisters())
    {
        co_return false;
    }

    if (!(co_await checkMTPCRC()))
    {
        co_return false;
    }

    co_return true;
}

} // namespace phosphor::software::VR
