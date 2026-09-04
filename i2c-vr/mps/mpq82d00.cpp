#include "mpq82d00.hpp"

#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

enum class MPQ82D00Cmd : uint8_t
{
    mfrConfigId = 0xB5,
    storeUserAll = 0x15,
    mfrUserCrc = 0xC8,
};

sdbusplus::async::task<bool> MPQ82D00::checkId()
{
    static constexpr size_t mfrIdReadLen = 4;
    static constexpr size_t mfrModelReadLen = 9;
    static constexpr size_t mfrConfigIdReadLen = 2;
    static constexpr uint32_t mpq82DevicePrefix = 0x3851504D;

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for ID check");
        co_return false;
    }

    tbuf = buildByteVector(PMBusCmd::mfrId);
    rbuf.resize(mfrIdReadLen);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read ID, cmd={CMD}", "CMD", lg2::hex,
              static_cast<uint8_t>(PMBusCmd::mfrId));
        co_return false;
    }

    auto vendorIdBytes = std::span(rbuf).subspan(1);
    auto vendorId = bytesToInt<uint32_t>(vendorIdBytes);
    if (vendorId != configuration->vendorId)
    {
        error("Vendor ID mismatch – got {GOT}, expected {EXP}", "GOT", lg2::hex,
              vendorId, "EXP", lg2::hex, configuration->vendorId);
        co_return false;
    }

    tbuf = buildByteVector(PMBusCmd::mfrModel);
    rbuf.clear();
    rbuf.resize(mfrModelReadLen);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read ID, cmd={CMD}", "CMD", lg2::hex,
              static_cast<uint8_t>(PMBusCmd::mfrModel));
        co_return false;
    }

    auto devicePrefix = bytesToInt<uint32_t>(std::span(rbuf).subspan(1, 4));
    if (devicePrefix != mpq82DevicePrefix)
    {
        error("Device ID mismatch – first 4 bytes got {GOT}, expected {EXP}",
              "GOT", lg2::hex, devicePrefix, "EXP", lg2::hex,
              mpq82DevicePrefix);
        co_return false;
    }

    tbuf = buildByteVector(MPQ82D00Cmd::mfrConfigId);
    rbuf.clear();
    rbuf.resize(mfrConfigIdReadLen);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read ID, cmd={CMD}", "CMD", lg2::hex,
              static_cast<uint8_t>(MPQ82D00Cmd::mfrConfigId));
        co_return false;
    }

    auto configId = bytesToInt<uint32_t>(rbuf);
    if (configId != configuration->configId)
    {
        error("Config ID mismatch – device has {GOT}, ATE file expects {EXP}",
              "GOT", lg2::hex, configId, "EXP", lg2::hex,
              configuration->configId);
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MPQ82D00::programRegisters()
{
    static constexpr auto page0Num = static_cast<uint8_t>(MPSPage::page0);

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set Page 0 for register programming");
        co_return false;
    }

    auto groupedData = getGroupedConfigData();

    if (groupedData.find(page0Num) == groupedData.end())
    {
        error("No Page-0 data in configuration");
        co_return false;
    }

    for (const auto& regData : groupedData.at(page0Num))
    {
        tbuf = {regData.addr};
        tbuf.insert(tbuf.end(), regData.data.begin(),
                    regData.data.begin() + regData.length);

        if (!i2cInterface.sendReceive(tbuf, rbuf))
        {
            error("Failed to write register {REG}", "REG", lg2::hex,
                  regData.addr);
            co_return false;
        }
    }

    co_return true;
}

sdbusplus::async::task<bool> MPQ82D00::storeMTP()
{
    static constexpr uint16_t mtpStoreWaitMs = 1500;

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set Page 0 for MTP store");
        co_return false;
    }

    tbuf = buildByteVector(MPQ82D00Cmd::storeUserAll);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to send STORE_USER_ALL command");
        co_return false;
    }

    // wait 1500 ms for the MTP write cycle to complete.
    co_await sdbusplus::async::sleep_for(
        ctx, std::chrono::milliseconds(mtpStoreWaitMs));

    co_return true;
}

sdbusplus::async::task<bool> MPQ82D00::getCRC(uint32_t* checksum)
{
    static constexpr size_t crcReadLen = 2;

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    PageGuard guard(i2cInterface);

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set Page 0 for CRC read");
        co_return false;
    }

    tbuf = buildByteVector(MPQ82D00Cmd::mfrUserCrc);
    rbuf.resize(crcReadLen);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read MFR_USER_CRC register");
        co_return false;
    }

    *checksum = bytesToInt<uint32_t>(rbuf);
    co_return true;
}

sdbusplus::async::task<bool> MPQ82D00::verifyCRC()
{
    uint32_t deviceCRC = 0;

    if (!co_await getCRC(&deviceCRC))
    {
        error("Failed to read CRC");
        co_return false;
    }

    info("Verify CRC: {DEV}", "DEV", lg2::hex, deviceCRC);

    co_return deviceCRC == configuration->crcUser;
}

sdbusplus::async::task<bool> MPQ82D00::parseDeviceConfiguration()
{
    static constexpr std::string_view mfrConfigIdRegName = "MFR_CONFIG_ID";
    static constexpr std::string_view mfrUserCrcRegName = "MFR_USER_CRC";
    static constexpr uint32_t mpsVendorId = 0x4D5053;

    if (!configuration)
    {
        error("Device configuration not initialized");
        co_return false;
    }

    configuration->vendorId = mpsVendorId;

    for (const auto& tokens : parser->lineTokens)
    {
        if (!parser->isValidDataTokens(tokens))
        {
            continue;
        }

        auto regName = parser->getVal<std::string>(tokens, ATE::regName);

        if (regName == mfrConfigIdRegName)
        {
            configuration->configId =
                parser->getVal<uint32_t>(tokens, ATE::configId);
        }
        else if (regName == mfrUserCrcRegName)
        {
            configuration->crcUser =
                parser->getVal<uint32_t>(tokens, ATE::regDataHex);
            break;
        }
    }

    co_return true;
}

sdbusplus::async::task<bool> MPQ82D00::verifyImage(const uint8_t* image,
                                                   size_t imageSize)
{
    if (!co_await parseImage(image, imageSize, MPSImageType::type1))
    {
        error("Image verification failed: image parsing failed");
        co_return false;
    }

    if (configuration->registersData.empty())
    {
        error("Image verification failed: no register data found in image");
        co_return false;
    }

    if (configuration->configId == 0)
    {
        error(
            "Image verification failed: MFR_CONFIG_ID row missing from ATE image");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MPQ82D00::updateFirmware(bool force)
{
    (void)force;

    PageGuard guard(i2cInterface);

    static constexpr size_t maxRetryCount = 2;

    if (!co_await checkId())
    {
        co_return false;
    }

    for (size_t attempt = 0; attempt < maxRetryCount; attempt++)
    {
        if (!co_await programRegisters())
        {
            co_return false;
        }

        if (!co_await storeMTP())
        {
            co_return false;
        }

        if (co_await verifyCRC())
        {
            co_return true;
        }
    }

    co_return false;
}

bool MPQ82D00::forcedUpdateAllowed()
{
    return true;
}

} // namespace phosphor::software::VR
