#include "mp297x.hpp"

#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

static constexpr size_t statusByteLength = 1;

static constexpr std::string_view crcUserRegName = "CRC_USER";
static constexpr std::string_view crcMultiRegName = "CRC_MULTI";

enum class MP297XCmd : uint8_t
{
    // Page 0 commands
    storeDataIntoMTP = 0xF1,

    // Page 1 commands
    writeProtectMode = 0x35,
    enableMTPPageWR = 0x4F,

    // Page 2 commands
    enableMultiConfigCRC = 0xF3,

    // Page 0x29 commands
    readUserCodeCRC = 0xFF,

    // Page 0x2A commands
    readMultiConfigCRC = 0xBF,
};

sdbusplus::async::task<bool> MP297X::parseDeviceConfiguration()
{
    if (!configuration)
    {
        error("Device configuration not initialized");
        co_return false;
    }

    configuration->vendorId = 0x0025;
    configuration->productId = 0x0071;

    for (const auto& tokens : parser->lineTokens)
    {
        if (!parser->isValidDataTokens(tokens))
        {
            continue;
        }

        auto regName = parser->getVal<std::string>(tokens, ATE::regName);

        if (regName == crcUserRegName)
        {
            configuration->configId =
                parser->getVal<uint32_t>(tokens, ATE::configId);
            configuration->crcUser =
                parser->getVal<uint32_t>(tokens, ATE::regDataHex);
        }
        else if (regName == crcMultiRegName)
        {
            configuration->crcMulti =
                parser->getVal<uint32_t>(tokens, ATE::regDataHex);
            break;
        }
    }

    co_return true;
}

sdbusplus::async::task<bool> MP297X::verifyImage(const uint8_t* image,
                                                 size_t imageSize)
{
    if (!co_await parseImage(image, imageSize))
    {
        error("Image verification failed: image parsing failed");
        co_return false;
    }

    if (configuration->registersData.empty())
    {
        error("Image verification failed - no data found");
        co_return false;
    }

    if (configuration->configId == 0)
    {
        error("Image verification failed - missing config ID");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MP297X::checkId(PMBusCmd idCmd, uint32_t expected)
{
    constexpr size_t idLength = 2;

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for ID check");
        co_return false;
    }

    tbuf = buildByteVector(idCmd);
    rbuf.resize(statusByteLength + idLength);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read ID, cmd={CMD}", "CMD", lg2::hex,
              static_cast<uint8_t>(idCmd));
        co_return false;
    }

    auto idBytes = std::span(rbuf).subspan(statusByteLength);
    auto id = bytesToInt<uint32_t>(idBytes);

    if (id != expected)
    {
        error("ID check failed for cmd {CMD}: got {ID}, expected {EXP}", "CMD",
              lg2::hex, static_cast<uint8_t>(idCmd), "ID", lg2::hex, id, "EXP",
              lg2::hex, expected);
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MP297X::isPasswordUnlock()
{
    constexpr uint8_t passwordMatchMask = 0x08;

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for password unlock check");
        co_return false;
    }

    tbuf = buildByteVector(PMBusCmd::statusCML);
    rbuf.resize(statusByteLength);

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to check password unlock status");
        co_return false;
    }

    auto unlocked = (rbuf[0] & passwordMatchMask) != 0;

    debug("Password unlock status: {STATUS}", "STATUS",
          unlocked ? "Unlocked" : "Locked");

    co_return unlocked;
}

sdbusplus::async::task<bool> MP297X::unlockWriteProtect()
{
    constexpr uint8_t writeProtectModeMask = 0x04;
    constexpr uint8_t unlockMemoryProtect = 0x00;
    constexpr uint8_t unlockMTPProtect = 0x63;

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf(statusByteLength);

    // Get write protection mode
    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page1);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 1 to check write protection mode");
        co_return false;
    }

    tbuf = buildByteVector(MP297XCmd::writeProtectMode);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to get write protect mode");
        co_return false;
    }

    auto isMTPMode = (rbuf[0] & writeProtectModeMask) == 0;
    auto unlockData = isMTPMode ? unlockMTPProtect : unlockMemoryProtect;

    // Unlock write protection
    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 to unlock write protection");
        co_return false;
    }

    tbuf = buildByteVector(PMBusCmd::writeProtect, unlockData);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to unlock write protection");
        co_return false;
    }

    debug("Unlocked {MODE} write protection", "MODE",
          isMTPMode ? "MTP" : "memory");
    co_return true;
}

sdbusplus::async::task<bool> MP297X::programPageRegisters(
    MPSPage page, const std::map<uint8_t, std::vector<MPSData>>& groupedData)
{
    auto pageNum = static_cast<uint8_t>(page);

    if (groupedData.find(pageNum) == groupedData.end())
    {
        debug("No data found for page {PAGE}", "PAGE", pageNum);
        co_return true;
    }

    const auto& data = groupedData.at(pageNum);

    // Page 2 is the multi-config page. Enter page2A to write config value
    // to MTP space directly during multi-config page programming.
    if (page == MPSPage::page2)
    {
        page = MPSPage::page2A;
        pageNum = static_cast<uint8_t>(page);
    }

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, page);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page {PAGE} to program registers", "PAGE",
              pageNum);
        co_return false;
    }

    auto i2cWriteWithRetry =
        [&](const std::vector<uint8_t>& tbuf) -> sdbusplus::async::task<bool> {
        std::vector<uint8_t> rbuf;
        constexpr size_t maxRetries = 3;
        constexpr auto retryDelay = std::chrono::milliseconds(10);

        for (size_t i = 1; i <= maxRetries; ++i)
        {
            if (i2cInterface.sendReceive(tbuf, rbuf))
            {
                co_return true;
            }
            error("I2C write failed, retry {RETRY}", "RETRY", i);
            co_await sdbusplus::async::sleep_for(ctx, retryDelay);
        }
        co_return false;
    };

    for (const auto& regData : data)
    {
        tbuf = {regData.addr};
        tbuf.insert(tbuf.end(), regData.data.begin(),
                    regData.data.begin() + regData.length);

        if (!(co_await i2cWriteWithRetry(tbuf)))
        {
            error(
                "Failed to write data {DATA} to register {REG} on page {PAGE}",
                "DATA", lg2::hex, bytesToInt<uint32_t>(regData.data), "REG",
                lg2::hex, regData.addr, "PAGE", pageNum);
            co_return false;
        }

        if (page == MPSPage::page2A)
        {
            // Page 2A requires a delay after each register write
            co_await sdbusplus::async::sleep_for(ctx,
                                                 std::chrono::milliseconds(2));
        }
    }

    debug("Programmed {N} registers on page {PAGE}", "N", data.size(), "PAGE",
          pageNum);

    co_return true;
}

sdbusplus::async::task<bool> MP297X::storeDataIntoMTP()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for storing data into MTP");
        co_return false;
    }

    tbuf = buildByteVector(MP297XCmd::storeDataIntoMTP);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to store data into MTP");
        co_return false;
    }

    // Wait store data into MTP
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::milliseconds(500));

    debug("Stored data into MTP");

    co_return true;
}

sdbusplus::async::task<bool> MP297X::enableMTPPageWriteRead()
{
    constexpr uint8_t mtpByteRWEnable = 0x20;

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page1);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 1 to enable MTP page write/read");
        co_return false;
    }

    tbuf = buildByteVector(MP297XCmd::enableMTPPageWR);
    rbuf.resize(statusByteLength);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read MTP page write/read status");
        co_return false;
    }

    uint8_t enableMTPPageWRData = rbuf[0] | mtpByteRWEnable;
    tbuf = buildByteVector(MP297XCmd::enableMTPPageWR, enableMTPPageWRData);
    rbuf.resize(0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to enable MTP page write/read");
        co_return false;
    }

    debug("Enabled MTP page write/read");
    co_return true;
}

sdbusplus::async::task<bool> MP297X::enableMultiConfigCRC()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page2);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 2 to enable multi-config CRC");
        co_return false;
    }

    tbuf = buildByteVector(MP297XCmd::enableMultiConfigCRC);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to enable multi-config CRC");
        co_return false;
    }

    debug("Enabled multi-config CRC");
    co_return true;
}

sdbusplus::async::task<bool> MP297X::getCRC(uint32_t& checksum)
{
    constexpr size_t crcLength = 2;

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    uint16_t userCodeCRC = 0;
    uint16_t multiConfigCRC = 0;

    // Read User Code CRC
    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page29);
    rbuf.resize(0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 29 for User Code CRC read");
        co_return false;
    }

    tbuf = buildByteVector(MP297XCmd::readUserCodeCRC);
    rbuf.resize(crcLength);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read User Code CRC from device");
        co_return false;
    }

    userCodeCRC = bytesToInt<uint16_t>(rbuf);

    // Read Multi Config CRC
    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page2A);
    rbuf.resize(0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 2A for Multi Config CRC read");
        co_return false;
    }

    tbuf = buildByteVector(MP297XCmd::readMultiConfigCRC);
    rbuf.resize(crcLength);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read Multi Config CRC from device");
        co_return false;
    }

    multiConfigCRC = bytesToInt<uint16_t>(rbuf);

    // Combine: [byte3 byte2] = userCodeCRC, [byte1 byte0] = multiConfigCRC
    checksum = (static_cast<uint32_t>(userCodeCRC) << 16) |
               static_cast<uint32_t>(multiConfigCRC);

    debug("Read CRC: {CRC}", "CRC", lg2::hex, checksum);

    co_return true;
}

sdbusplus::async::task<bool> MP297X::checkMTPCRC()
{
    uint32_t crc = 0;
    auto expectedCRC = (configuration->crcUser << 16) | configuration->crcMulti;

    if (!co_await getCRC(crc))
    {
        error("Failed to get CRC for MTP check");
        co_return false;
    }

    debug("MTP CRC: {CRC}, Expected: {EXP}", "CRC", lg2::hex, crc, "EXP",
          lg2::hex, expectedCRC);

    co_return crc == expectedCRC;
}

bool MP297X::forcedUpdateAllowed()
{
    return true;
}

sdbusplus::async::task<bool> MP297X::updateFirmware(bool force)
{
    (void)force;

    auto groupedConfigData = getGroupedConfigData();

    if (!co_await checkId(PMBusCmd::mfrId, configuration->vendorId))
    {
        co_return false;
    }

    if (!co_await checkId(PMBusCmd::mfrModel, configuration->productId))
    {
        co_return false;
    }

    if (!co_await isPasswordUnlock())
    {
        co_return false;
    }

    if (!co_await unlockWriteProtect())
    {
        co_return false;
    }

    if (!co_await programPageRegisters(MPSPage::page0, groupedConfigData))
    {
        co_return false;
    }

    if (!co_await programPageRegisters(MPSPage::page1, groupedConfigData))
    {
        co_return false;
    }

    if (!co_await storeDataIntoMTP())
    {
        co_return false;
    }

    if (!co_await enableMTPPageWriteRead())
    {
        co_return false;
    }

    if (!co_await enableMultiConfigCRC())
    {
        co_return false;
    }

    if (!co_await programPageRegisters(MPSPage::page2, groupedConfigData))
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
