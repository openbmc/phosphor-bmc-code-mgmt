#include "mp994x.hpp"

#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

static constexpr std::string_view vendorIdRegName = "VENDOR_ID_VR";
static constexpr std::string_view mfrDeviceIDCFGRegName = "MFR_DEVICE_ID_CFG";
static constexpr std::string_view crcUserMultiRegName = "CRC_USER_MULTI";

static constexpr uint8_t pageMask = 0x0F;

enum class MP994XCmd : uint8_t
{
    // Page 0 commands
    storeUserAll = 0x15,
    userData08 = 0xB8,

    // Page 2 commands
    mfrMulconfigSel = 0xAB,
    configId = 0xAF,
    mfrNVMPmbusCtrl = 0xCA,
    mfrDebug = 0xD4,
    deviceId = 0xDB,

    // Page 5 commands
    vensorId = 0xBA,

    // Page 7 commands
    storeFaultTrigger = 0x51,
};

sdbusplus::async::task<bool> MP994X::parseDeviceConfiguration()
{
    if (!configuration)
    {
        error("Device configuration not initialized");
        co_return false;
    }

    for (const auto& tokens : parser->lineTokens)
    {
        if (!parser->isValidDataTokens(tokens))
        {
            continue;
        }

        auto regName = parser->getVal<std::string>(tokens, ATE::regName);

        if (regName == vendorIdRegName)
        {
            configuration->vendorId =
                parser->getVal<uint32_t>(tokens, ATE::regDataHex);
            configuration->configId =
                parser->getVal<uint32_t>(tokens, ATE::configId);
        }
        else if (regName == mfrDeviceIDCFGRegName)
        {
            configuration->productId =
                parser->getVal<uint32_t>(tokens, ATE::regDataHex);
        }
        else if (regName == crcUserMultiRegName)
        {
            configuration->crcMulti =
                parser->getVal<uint32_t>(tokens, ATE::regDataHex);
            break;
        }
    }

    co_return true;
}

sdbusplus::async::task<bool> MP994X::verifyImage(const uint8_t* image,
                                                 size_t imageSize)
{
    if (!co_await parseImage(image, imageSize, MPSImageType::type1))
    {
        error("Image verification failed: image parsing failed");
        co_return false;
    }

    if (configuration->registersData.empty())
    {
        error("Image verification failed: no data found");
        co_return false;
    }

    if (configuration->vendorId == 0 || configuration->productId == 0 ||
        configuration->configId == 0)
    {
        error("Image verification failed: missing required field "
              "vendor ID, product ID, or config ID");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MP994X::checkId(MP994XCmd idCmd, uint32_t expected)
{
    static constexpr size_t vendorIdLength = 2;
    static constexpr size_t productIdLength = 1;
    static constexpr size_t configIdLength = 2;

    MPSPage page;
    size_t idLen = 0;
    const uint8_t cmd = static_cast<uint8_t>(idCmd);

    switch (idCmd)
    {
        case MP994XCmd::vensorId:
            page = MPSPage::page5;
            idLen = vendorIdLength;
            break;
        case MP994XCmd::deviceId:
            page = MPSPage::page2;
            idLen = productIdLength;
            break;
        case MP994XCmd::configId:
            page = MPSPage::page2;
            idLen = configIdLength;
            break;
        default:
            error("Invalid command for ID check: {CMD}", "CMD", lg2::hex, cmd);
            co_return false;
    }

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, page);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page for ID check");
        co_return false;
    }

    tbuf = buildByteVector(idCmd);
    rbuf.resize(idLen);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read ID, cmd={CMD}", "CMD", lg2::hex, cmd);
        co_return false;
    }

    auto id = bytesToInt<uint32_t>(rbuf);

    if (id != expected)
    {
        error("ID check failed for cmd {CMD}: got {ID}, expected {EXP}", "CMD",
              lg2::hex, cmd, "ID", lg2::hex, id, "EXP", lg2::hex, expected);
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MP994X::unlockWriteProtect()
{
    static constexpr uint8_t unlockWriteProtectData = 0x00;

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 to unlock write protection mode");
        co_return false;
    }

    tbuf = buildByteVector(PMBusCmd::writeProtect, unlockWriteProtectData);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to unlock write protect mode");
        co_return false;
    }

    debug("Write protection unlocked");
    co_return true;
}

sdbusplus::async::task<bool> MP994X::disableStoreFaultTriggering()
{
    static constexpr size_t mfrDebugDataLength = 2;
    static constexpr uint16_t enableEnteringPage7Mask = 0x8000;
    static constexpr uint16_t disableStoreFaultTriggeringData = 0x1000;

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    // enable entering page 7
    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page2);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 2 to enable entering page 7");
        co_return false;
    }

    tbuf = buildByteVector(MP994XCmd::mfrDebug);
    rbuf.resize(mfrDebugDataLength);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read MFR Debug register to enable entering 7");
        co_return false;
    }

    uint16_t data = (rbuf[1] << 8) | rbuf[0] | enableEnteringPage7Mask;
    tbuf = buildByteVector(MP994XCmd::mfrDebug, data);
    rbuf.clear();
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to enable entering page 7");
        co_return false;
    }

    // disable store fault triggering
    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page7);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 7 to disable store fault triggering");
        co_return false;
    }

    tbuf = buildByteVector(MP994XCmd::storeFaultTrigger,
                           disableStoreFaultTriggeringData);
    rbuf.clear();
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to disable store fault triggering");
        co_return false;
    }

    debug("Disabled store fault triggerring");

    co_return true;
}

sdbusplus::async::task<bool> MP994X::setMultiConfigAddress(uint8_t config)
{
    // MPS994X: Select multi-configuration address
    // Write to Page 2 @ 0xAB:
    //   - Bit[3] = MFR_MULCONFIG_SEL (1 = enable)
    //   - Bit[2:0] = MFR_MULCONFIG_ADDR (0 ~ 7 → selects one of 8 configs)
    // Resulting values for config set 1 ~ 8: 0x08 ~ 0x0F
    auto addr = config - 1;

    if (addr > 7)
    {
        error("Invalid multi config address: {ADDR}", "ADDR", addr);
    }

    static constexpr uint8_t enableMultiConfigAddrSel = 0x08;

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page2);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 2 to set multi config address");
        co_return false;
    }

    uint8_t selectAddrData = enableMultiConfigAddrSel + addr;
    tbuf = buildByteVector(MP994XCmd::mfrMulconfigSel, selectAddrData);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to write {DATA} to multi config select register {REG}",
              "DATA", lg2::hex, selectAddrData, "REG", lg2::hex,
              static_cast<uint8_t>(MP994XCmd::mfrMulconfigSel));
        co_return false;
    }

    debug("Selected multi config set address {ADDR}", "ADDR", addr);
    co_return true;
}

sdbusplus::async::task<bool> MP994X::programConfigData(
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

    if (!co_await storeDataIntoMTP())
    {
        error("Failed to store code into MTP after programming config data");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MP994X::programAllRegisters()
{
    // config 0 = User Registers
    // config 1 ~ 8 = Multi-configuration Registers
    for (const auto& [config, gdata] : getGroupedConfigData(~pageMask, 4))
    {
        debug("Configuring registers for config set {SET}", "SET", config);

        if (config > 0)
        {
            if (!co_await setMultiConfigAddress(config))
            {
                co_return false;
            }
        }

        if (!co_await programConfigData(gdata))
        {
            error("Failed to program config set {SET}", "SET", config);
            co_return false;
        }

        debug("Configured {SIZE} registers for config set {SET}", "SIZE",
              gdata.size(), "SET", config);
    }

    debug("All registers were programmed successfully");

    co_return true;
}

sdbusplus::async::task<bool> MP994X::storeDataIntoMTP()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 to store data into MTP");
        co_return false;
    }

    tbuf = buildByteVector(MP994XCmd::storeUserAll);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to store data into MTP");
        co_return false;
    }

    // Wait store data
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::milliseconds(1000));

    debug("Stored data into MTP");
    co_return true;
}

sdbusplus::async::task<bool> MP994X::getCRC(uint32_t* checksum)
{
    static constexpr size_t crcUserMultiDataLength = 4;
    static constexpr size_t statusByteLength = 1;

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for get user data");
        co_return false;
    }

    tbuf = buildByteVector(MP994XCmd::userData08);
    rbuf.resize(crcUserMultiDataLength + statusByteLength);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to get user data on page 0");
        co_return false;
    }

    auto crcBytes = std::span(rbuf).subspan(statusByteLength);
    *checksum = bytesToInt<uint32_t>(crcBytes);

    debug("Read CRC: {CRC}", "CRC", lg2::hex, *checksum);
    co_return true;
}

sdbusplus::async::task<bool> MP994X::restoreDataFromNVM()
{
    static constexpr size_t nvmPmbusCtrlDataLength = 2;
    static constexpr uint16_t enableRestoreDataFromMTPMask = 0x0008;

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    // enable restore data from MTP
    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page2);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 2 to enable restore data from MTP");
        co_return false;
    }

    tbuf = buildByteVector(MP994XCmd::mfrNVMPmbusCtrl);
    rbuf.resize(nvmPmbusCtrlDataLength);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read NVM PMBUS Ctrl register");
        co_return false;
    }

    uint16_t data = ((rbuf[1] << 8) | rbuf[0]) | enableRestoreDataFromMTPMask;
    tbuf = buildByteVector(MP994XCmd::mfrNVMPmbusCtrl, data);
    rbuf.clear();
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to enable restore data from MTP");
        co_return false;
    }

    // restore data from NVM
    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for restore MTP and verify");
    }

    tbuf = buildByteVector(PMBusCmd::restoreUserAll);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to restore data from NVM");
        co_return false;
    }

    // wait restore data
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::milliseconds(500));

    debug("Restored data from NVM success");

    co_return true;
}

sdbusplus::async::task<bool> MP994X::checkMTPCRC()
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
          lg2::hex, configuration->crcMulti);

    co_return configuration->crcMulti == crc;
}

bool MP994X::forcedUpdateAllowed()
{
    return true;
}

sdbusplus::async::task<bool> MP994X::updateFirmware(bool force)
{
    (void)force;

    if (!configuration)
    {
        error("Configuration not loaded");
        co_return false;
    }

    if (!co_await checkId(MP994XCmd::vensorId, configuration->vendorId))
    {
        co_return false;
    }

    if (!co_await checkId(MP994XCmd::deviceId, configuration->productId))
    {
        co_return false;
    }

    if (!co_await checkId(MP994XCmd::configId, configuration->configId))
    {
        co_return false;
    }

    if (!co_await unlockWriteProtect())
    {
        co_return false;
    }

    if (!co_await disableStoreFaultTriggering())
    {
        co_return false;
    }

    if (!co_await programAllRegisters())
    {
        co_return false;
    }

    if (!co_await restoreDataFromNVM())
    {
        co_return false;
    }

    if (!co_await checkMTPCRC())
    {
        co_return false;
    }

    co_return true;
}

} // namespace phosphor::software::VR
