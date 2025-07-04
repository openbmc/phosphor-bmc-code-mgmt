#include "mp2x6xx.hpp"

#include "common/include/utils.hpp"
#include "mps.hpp"

#include <boost/algorithm/string.hpp>
#include <phosphor-logging/lg2.hpp>

#include <sstream>
#include <string_view>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

static constexpr uint32_t mpsVendorId = 0x4D5053;

static constexpr size_t vendorIdLength = 3;
static constexpr size_t deviceIdLength = 4;
static constexpr size_t configIdLength = 2;
static constexpr size_t statusByteLength = 1;
static constexpr size_t crcLength = 2;

static constexpr std::string_view dataEndTag = "END";
static constexpr std::string_view productIdRegName = "TRIM_MFR_PRODUCT_ID2";
static constexpr std::string_view userCRCRegName = "CRC_USER";

static constexpr uint8_t pageMask = 0x0F;
static constexpr uint8_t configMask = 0xF0;

static constexpr uint8_t disableWriteProtect = 0x00;
static constexpr uint16_t disablePage2WriteProtect = 0x128C;
static constexpr uint16_t disablePage3WriteProtect = 0x0082;

struct MP2X6XXData
{
    uint8_t page = 0;
    uint8_t addr = 0;
    std::array<uint8_t, 4> data{};
    uint8_t length = 0;
};

struct MP2X6XXConfig
{
    uint32_t vendorId = 0;
    uint32_t deviceId = 0;
    uint32_t configId = 0;
    uint32_t crc = 0;
    std::vector<MP2X6XXData> pdata;
};

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

class MP2X6XXImageParser
{
  public:
    static std::shared_ptr<MP2X6XXConfig> parse(const uint8_t* image,
                                                size_t imageSize)
    {
        auto config = std::make_shared<MP2X6XXConfig>();
        config->vendorId = mpsVendorId;

        std::istringstream stream(
            std::string(reinterpret_cast<const char*>(image), imageSize));
        std::string line;
        bool parsingUserData = true;

        while (std::getline(stream, line))
        {
            std::vector<std::string> tokens;
            std::stringstream lineStream(line);
            std::string token;
            while (std::getline(lineStream, token, '\t'))
            {
                tokens.push_back(token);
            }

            if (line.empty() || line.starts_with('*') ||
                tokens.size() != static_cast<size_t>(ATE::colCount))
            {
                if (line.starts_with(dataEndTag))
                {
                    parsingUserData = false;
                }
                continue;
            }

            auto regName = getVal<std::string>(tokens, ATE::regName);

            if (regName == productIdRegName)
            {
                config->deviceId = getVal<uint32_t>(tokens, ATE::regDataHex);
                config->configId = getVal<uint32_t>(tokens, ATE::configId);
            }

            if (parsingUserData)
            {
                auto data = extractData(tokens);
                if (data.length > 0)
                {
                    config->pdata.emplace_back(data);
                }
            }
            else if (regName == userCRCRegName)
            {
                config->crc = getVal<uint32_t>(tokens, ATE::regDataHex);
                break;
            }
        }

        debug("Parsed MP2X6XX config: Vendor ID={VID}, Device ID={DID}, "
              "Config ID={CID}, Data Size={SIZE}, CRC={CRC}",
              "VID", lg2::hex, config->vendorId, "DID", lg2::hex,
              config->deviceId, "CID", lg2::hex, config->configId, "SIZE",
              config->pdata.size(), "CRC", lg2::hex, config->crc);

        return config;
    }

  private:
    template <typename T>
    static T getVal(const std::vector<std::string>& tokens, ATE index)
    {
        if (tokens.size() <= static_cast<size_t>(index))
        {
            error("Index out of range for ATE enum: {INDEX}", "INDEX",
                  static_cast<uint8_t>(index));
            return T{};
        }

        const std::string& token = tokens[static_cast<size_t>(index)];

        if constexpr (std::is_same_v<T, std::string>)
        {
            return token;
        }
        else if constexpr (std::is_integral_v<T>)
        {
            return static_cast<T>(std::stoul(token, nullptr, 16));
        }
        else
        {
            static_assert(always_false<T>, "Unsupported type in getVal");
        }
    }

    static MP2X6XXData extractData(const std::vector<std::string>& tokens)
    {
        MP2X6XXData data;
        data.page = getVal<uint8_t>(tokens, ATE::pageNum);
        data.addr = getVal<uint8_t>(tokens, ATE::regAddrHex);
        std::string regData = getVal<std::string>(tokens, ATE::regDataHex);
        size_t byteCount = std::min(regData.length() / 2, size_t(4));

        for (size_t i = 0; i < byteCount; ++i)
        {
            data.data[byteCount - 1 - i] = static_cast<uint8_t>(
                std::stoul(regData.substr(i * 2, 2), nullptr, 16));
        }

        data.length = static_cast<uint8_t>(byteCount);
        return data;
    }
};

sdbusplus::async::task<bool> MP2X6XX::verifyImage(const uint8_t* image,
                                                  size_t imageSize)
{
    configuration = MP2X6XXImageParser::parse(image, imageSize);

    if (configuration->pdata.empty())
    {
        error("Image verification failed - no data found");
        co_return false;
    }

    if (configuration->configId == 0 || configuration->deviceId == 0)
    {
        error("Image verification failed - missing device or config ID");
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

    debug("Check ID cmd {CMD}: Got={ID}, Expected={EXP}", "CMD", lg2::hex, cmd,
          "ID", lg2::hex, id, "EXP", lg2::hex, expected);

    co_return id == expected;
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

std::map<uint8_t, std::vector<MP2X6XXData>> MP2X6XX::getGroupedConfigData()
{
    std::map<uint8_t, std::vector<MP2X6XXData>> grouped;

    for (const auto& data : configuration->pdata)
    {
        uint8_t config = (data.page & configMask) >> 4;
        grouped[config].push_back(data);
    }

    return grouped;
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
    const std::vector<MP2X6XXData>& gdata)
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
    for (const auto& [config, gdata] : getGroupedConfigData())
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
          lg2::hex, configuration->crc);

    co_return configuration->crc == crc;
}

bool MP2X6XX::forcedUpdateAllowed()
{
    return true;
}

sdbusplus::async::task<bool> MP2X6XX::reset()
{
    // MPS not support reset command, so we just return true
    co_return true;
}

sdbusplus::async::task<bool> MP2X6XX::updateFirmware(bool force)
{
    (void)force;

    if (!co_await checkId(PMBusCmd::mfrId, configuration->vendorId))
    {
        co_return false;
    }

    if (!co_await checkId(PMBusCmd::icDeviceId, configuration->deviceId))
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
