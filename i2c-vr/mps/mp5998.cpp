#include "mp5998.hpp"

#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

static constexpr std::string_view crcUserRegName = "CRC_USER";
static constexpr std::string_view crcConfigId = "CONFIG_ID";
static constexpr uint8_t eepromFaultBit = 0x01;
static constexpr uint8_t unlockData = 0x00;
static constexpr size_t statusByteLength = 1;

enum class MP5998Cmd : uint8_t
{
    vendorId = 0x99,
    productId = 0xB0,
    crcUser = 0xF8,
    passwordReg = 0x0E,
    statusCml = 0x7E,
    storeMTP = 0x17,
    restoreMTP = 0x16,
};

sdbusplus::async::task<bool> MP5998::parseDeviceConfiguration()
{
    if (!configuration)
    {
        error("Device configuration not initialized");
        co_return false;
    }

    configuration->vendorId = 0x53504D;
    configuration->productId = 0x5998;
    configuration->configId = 0x001C;
    configuration->crcUser = 0xFCFF;

    for (const auto& tokens : parser->lineTokens)
    {
        if (!parser->isValidDataTokens(tokens))
        {
            continue;
        }

        auto regName = parser->getVal<std::string>(tokens, ATE::regName);

        if (regName == crcUserRegName)
        {
            configuration->crcUser =
                parser->getVal<uint32_t>(tokens, ATE::regDataHex);
        }

        if (regName == crcConfigId)
        {
            configuration->configId =
                parser->getVal<uint32_t>(tokens, ATE::regDataHex);
        }
    }

    co_return true;
}

sdbusplus::async::task<bool> MP5998::verifyImage(const uint8_t* image,
                                                 size_t imageSize)
{
    if (!co_await parseImage(image, imageSize))
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

sdbusplus::async::task<bool> MP5998::checkId(uint8_t cmd, uint32_t expected)
{
    constexpr size_t vendorIdLength = 3;
    constexpr size_t productIdLength = 2;

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for ID check");
        co_return false;
    }

    if (cmd == static_cast<uint8_t>(PMBusCmd::mfrId))
    {
        tbuf = buildByteVector(MP5998Cmd::vendorId);
        rbuf.resize(vendorIdLength + statusByteLength);

        if (!i2cInterface.sendReceive(tbuf, rbuf))
        {
            error("Failed to read vendor ID");
            co_return false;
        }

        uint32_t actual = (static_cast<uint32_t>(rbuf[1]) << 16) |
                          (static_cast<uint32_t>(rbuf[2]) << 8) |
                          static_cast<uint32_t>(rbuf[3]);

        co_return expected == actual;
    }
    else if (cmd == static_cast<uint8_t>(MP5998Cmd::productId))
    {
        tbuf = buildByteVector(MP5998Cmd::productId);
        rbuf.resize(productIdLength);

        if (!i2cInterface.sendReceive(tbuf, rbuf))
        {
            error("Failed to read product ID");
            co_return false;
        }

        uint32_t actual = bytesToInt<uint32_t>(rbuf);

        co_return expected == actual;
    }
    else
    {
        error("Unsupported ID command: 0x{CMD}", "CMD", lg2::hex, cmd);
        co_return false;
    }
}

sdbusplus::async::task<bool> MP5998::unlockPasswordProtection()
{
    constexpr uint8_t passwordUnlockBit = 0x08;
    constexpr uint16_t passwordData = 0x0000;

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for password unlock");
        co_return false;
    }

    tbuf = buildByteVector(MP5998Cmd::passwordReg, passwordData);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to write password");
        co_return false;
    }

    tbuf = buildByteVector(MP5998Cmd::statusCml);
    rbuf.resize(statusByteLength);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read STATUS_CML");
        co_return false;
    }

    bool unlocked = (rbuf[0] & passwordUnlockBit) == 0;

    co_return unlocked;
}

sdbusplus::async::task<bool> MP5998::unlockWriteProtection()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for write protection unlock");
        co_return false;
    }

    tbuf = buildByteVector(PMBusCmd::writeProtect, unlockData);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to unlock write protection");
        co_return false;
    }

    debug("Write protection unlocked");
    co_return true;
}

sdbusplus::async::task<bool> MP5998::programAllRegisters()
{
    uint8_t currentPage = 0xFF;

    for (const auto& regData : configuration->registersData)
    {
        if (regData.page != currentPage)
        {
            std::vector<uint8_t> tbuf =
                buildByteVector(PMBusCmd::page, regData.page);
            std::vector<uint8_t> rbuf;
            if (!i2cInterface.sendReceive(tbuf, rbuf))
            {
                error("Failed to set page {PAGE}", "PAGE", regData.page);
                co_return false;
            }
            currentPage = regData.page;
        }

        std::vector<uint8_t> tbuf;
        std::vector<uint8_t> rbuf;

        tbuf.push_back(regData.addr);

        for (uint8_t i = 0; i < regData.length && i < 4; ++i)
        {
            tbuf.push_back(regData.data[i]);
        }

        if (!i2cInterface.sendReceive(tbuf, rbuf))
        {
            error("Failed to write register 0x{REG} on page {PAGE}", "REG",
                  lg2::hex, regData.addr, "PAGE", regData.page);
            co_return false;
        }
    }

    debug("All registers programmed successfully");
    co_return true;
}

sdbusplus::async::task<bool> MP5998::storeMTP()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for MTP store");
        co_return false;
    }

    tbuf = buildByteVector(MP5998Cmd::storeMTP);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to send STORE_USER_ALL command");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MP5998::waitForMTPComplete()
{
    constexpr uint16_t mtpStoreWaitmS = 1200;
    co_await sdbusplus::async::sleep_for(
        ctx, std::chrono::milliseconds(mtpStoreWaitmS));
    std::vector<uint8_t> tbuf = buildByteVector(PMBusCmd::statusCML);
    std::vector<uint8_t> rbuf;
    rbuf.resize(statusByteLength);

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read STATUS_CML after MTP store");
        co_return false;
    }

    bool eepromFault = rbuf[0] & eepromFaultBit;

    if (eepromFault)
    {
        error("EEPROM fault detected after MTP store");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MP5998::verifyCRC()
{
    uint32_t deviceCRC = 0;
    auto crcResult = co_await getCRC(&deviceCRC);
    if (!crcResult)
    {
        error("Failed to read CRC from device");
        co_return false;
    }

    bool crcMatch = (deviceCRC == configuration->crcUser);

    co_return crcMatch;
}

sdbusplus::async::task<bool> MP5998::getCRC(uint32_t* checksum)
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

    tbuf = buildByteVector(MP5998Cmd::crcUser);
    rbuf.resize(crcLength);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read CRC_USER register");
        co_return false;
    }

    *checksum = bytesToInt<uint32_t>(rbuf);

    co_return true;
}

sdbusplus::async::task<bool> MP5998::sendRestoreMTPCommand()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for MTP restore");
        co_return false;
    }

    tbuf = buildByteVector(MP5998Cmd::restoreMTP);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to send RESTORE_ALL command");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MP5998::checkEEPROMFaultAfterRestore()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to set page 0 for EEPROM fault check");
        co_return false;
    }

    tbuf = buildByteVector(PMBusCmd::statusCML);
    rbuf.resize(1);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to read STATUS_CML register");
        co_return false;
    }

    bool eepromFault = (rbuf[0] & eepromFaultBit) != 0;

    co_return !eepromFault;
}

sdbusplus::async::task<bool> MP5998::restoreMTPAndVerify()
{
    constexpr uint16_t mtpRestoreWait = 1600;

    if (!co_await sendRestoreMTPCommand())
    {
        error("Failed to send RESTORE_ALL command");
        co_return false;
    }

    co_await sdbusplus::async::sleep_for(
        ctx, std::chrono::microseconds(mtpRestoreWait));
    if (!co_await checkEEPROMFaultAfterRestore())
    {
        error("EEPROM fault detected after MTP restore");
        co_return false;
    }

    co_return true;
}

bool MP5998::forcedUpdateAllowed()
{
    return true;
}

sdbusplus::async::task<bool> MP5998::reset()
{
    co_return true;
}

sdbusplus::async::task<bool> MP5998::update()
{
    if (!co_await checkId(static_cast<uint8_t>(PMBusCmd::mfrId),
                          configuration->vendorId))
    {
        co_return false;
    }

    if (!co_await checkId(static_cast<uint8_t>(MP5998Cmd::productId),
                          configuration->productId))
    {
        co_return false;
    }

    if (!co_await unlockPasswordProtection())
    {
        co_return false;
    }

    if (!co_await unlockWriteProtection())
    {
        co_return false;
    }

    if (!co_await programAllRegisters())
    {
        co_return false;
    }

    if (!co_await storeMTP())
    {
        co_return false;
    }

    if (!co_await waitForMTPComplete())
    {
        co_return false;
    }

    if (!co_await verifyCRC())
    {
        co_return false;
    }

    if (!co_await restoreMTPAndVerify())
    {
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MP5998::updateFirmware(bool force)
{
    (void)force;

    if (!configuration)
    {
        error("Configuration not loaded");
        co_return false;
    }

    auto status = co_await update();

    co_return status;
}

} // namespace phosphor::software::VR
