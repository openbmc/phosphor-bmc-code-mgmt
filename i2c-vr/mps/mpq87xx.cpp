#include "mpq87xx.hpp"

#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

enum class MPQ87XXCmd : uint8_t
{
    storeAll = 0x15,
    restoreAll = 0x16,
    mfrConfigId = 0xc0,
    mfrConfigCodeRev = 0xc1,
    checksumFunc = 0xf8,
};

sdbusplus::async::task<bool> MPQ87XX::checkId()
{
    static constexpr size_t mfrIdLength = 4;
    static constexpr size_t mfrConfigIdLength = 2;
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for ID check");
        co_return false;
    }

    tbuf = buildByteVector(PMBusCmd::mfrId);
    rbuf.resize(mfrIdLength);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read ID, cmd={CMD}", "CMD", lg2::hex,
              static_cast<uint8_t>(PMBusCmd::mfrId));
        co_return false;
    }

    auto idBytes = std::span(rbuf).subspan(1);
    auto id = bytesToInt<uint32_t>(idBytes);

    if (id != configuration->vendorId)
    {
        error("ID check failed for cmd {CMD}: got {ID}, expected {EXP}", "CMD",
              lg2::hex, static_cast<uint8_t>(PMBusCmd::mfrId), "ID", lg2::hex,
              id, "EXP", lg2::hex, configuration->vendorId);
        co_return false;
    }

    tbuf = buildByteVector(MPQ87XXCmd::mfrConfigId);
    rbuf.clear();
    rbuf.resize(mfrConfigIdLength);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read ID, cmd={CMD}", "CMD", lg2::hex,
              static_cast<uint8_t>(MPQ87XXCmd::mfrConfigId));
        co_return false;
    }

    id = bytesToInt<uint32_t>(rbuf);
    if (id != configuration->productId)
    {
        error("ID check failed for cmd {CMD}: got {ID}, expected {EXP}", "CMD",
              lg2::hex, static_cast<uint8_t>(MPQ87XXCmd::mfrConfigId), "ID",
              lg2::hex, id, "EXP", lg2::hex, configuration->vendorId);
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MPQ87XX::programPageRegisters(
    MPSPage page, const std::map<uint8_t, std::vector<MPSData>>& groupedData)
{
    auto pageNum = static_cast<uint8_t>(page);

    if (groupedData.find(pageNum) == groupedData.end())
    {
        error("No data found for page {PAGE}", "PAGE", pageNum);
        co_return true;
    }

    const auto& data = groupedData.at(pageNum);

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, page);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page {PAGE} to program registers", "PAGE",
              pageNum);
        co_return false;
    }

    for (const auto& regData : data)
    {
        tbuf = {regData.addr};
        tbuf.insert(tbuf.end(), regData.data.begin(),
                    regData.data.begin() + regData.length);

        if (!i2cInterface.sendReceive(tbuf, rbuf))
        {
            error("Failed to program VR registers");
            co_return false;
        }
    }

    co_return true;
}

sdbusplus::async::task<bool> MPQ87XX::storeMTP()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for MTP store");
        co_return false;
    }

    tbuf = buildByteVector(MPQ87XXCmd::storeAll);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to send STORE_USER_ALL command");
        co_return false;
    }

    constexpr uint16_t mtpStoreWaitmS = 500;
    co_await sdbusplus::async::sleep_for(
        ctx, std::chrono::milliseconds(mtpStoreWaitmS));

    co_return true;
}

sdbusplus::async::task<bool> MPQ87XX::parseDeviceConfiguration()
{
    static constexpr std::string_view productIdRegName = "MFR_CONFIG_ID";
    static constexpr std::string_view crcUserRegName = "CRC_USER";
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

sdbusplus::async::task<bool> MPQ87XX::verifyCRC()
{
    uint32_t deviceCRC = 0;
    bool getCRCSuccess = co_await getCRC(&deviceCRC);

    if (!getCRCSuccess)
    {
        error("Failed to read CRC from device");
        co_return false;
    }

    info("verify CRC: {ID}", "ID", lg2::hex, deviceCRC);

    bool crcMatch = (deviceCRC == configuration->crcUser);

    co_return crcMatch;
}

sdbusplus::async::task<bool> MPQ87XX::verifyImage(const uint8_t* image,
                                                  size_t imageSize)
{
    if (!co_await parseImage(image, imageSize, MPSImageType::type0))
    {
        error("Image verification failed: image parsing failed");
        co_return false;
    }

    if (configuration->registersData.empty())
    {
        error("Image verification failed - no register data found");
        co_return false;
    }

    if (configuration->configId == 0)
    {
        error("Image verification failed - missing config ID");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MPQ87XX::getCRC(uint32_t* checksum)
{
    constexpr size_t crcLength = 2;

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for CRC read");
        co_return false;
    }

    tbuf = buildByteVector(MPQ87XXCmd::checksumFunc);
    rbuf.resize(crcLength);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read CRC register");
        co_return false;
    }

    *checksum = bytesToInt<uint32_t>(rbuf);

    co_return true;
}

bool MPQ87XX::forcedUpdateAllowed()
{
    return true;
}

sdbusplus::async::task<bool> MPQ87XX::updateFirmware(bool force)
{
    (void)force;

    auto groupedConfigData = getGroupedConfigData();
    static constexpr size_t maxRetryCount = 2;

    if (!co_await checkId())
    {
        co_return false;
    }

    for (size_t retryCount = 0; retryCount < maxRetryCount; retryCount++)
    {
        if (!co_await programPageRegisters(MPSPage::page0, groupedConfigData))
        {
            co_return false;
        }

        if (!co_await storeMTP())
        {
            co_return false;
        }

        // Per the update guide, retry if a failure occurs.
        if (co_await verifyCRC())
        {
            co_return true;
        }
    }

    co_return false;
}

} // namespace phosphor::software::VR
