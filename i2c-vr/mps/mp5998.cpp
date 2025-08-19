#include "mp5998.hpp"

#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

static constexpr std::string_view crcUserRegName = "CRC_USER";
static constexpr uint8_t eepromFaultBit = 0x01;
static constexpr uint8_t unlockData = 0x00;
static constexpr size_t statusByteLength = 1;

enum class MP5998Cmd : uint8_t
{
    crcUser = 0xF8,
    passwordReg = 0x0E,
};

sdbusplus::async::task<bool> MP5998::parseDeviceConfiguration()
{
    if (!configuration)
    {
        error("Device configuration not initialized");
        co_return false;
    }

    configuration->vendorId = 0x4D5053;
    configuration->productId = 0x35393938;

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

sdbusplus::async::task<bool> MP5998::checkId(PMBusCmd idCmd, uint32_t expected)
{
    static constexpr size_t mfrIdLength = 3;
    static constexpr size_t mfrModelLength = 5;

    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, MPSPage::page0);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("MP5998: Failed to set page 0 for ID check");
        co_return false;
    }

    size_t bufferSize;

    if (idCmd == PMBusCmd::mfrId)
    {
        bufferSize = statusByteLength + mfrIdLength;
    }
    else if (idCmd == PMBusCmd::mfrModel)
    {
        bufferSize = statusByteLength + mfrModelLength;
    }
    else
    {
        error("MP5998: Unsupported ID command: 0x{CMD}", "CMD", lg2::hex,
              static_cast<uint8_t>(idCmd));
        co_return false;
    }

    tbuf = buildByteVector(idCmd);
    rbuf.resize(bufferSize);

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("MP5998: I2C sendReceive failed for command 0x{CMD}", "CMD",
              lg2::hex, static_cast<uint8_t>(idCmd));
        co_return false;
    }

    auto idBytes = std::span(rbuf).subspan(statusByteLength);
    uint32_t id;
    if (idCmd == PMBusCmd::mfrModel)
    {
        auto productBytes = idBytes.subspan(1, 4);
        id = bytesToInt<uint32_t>(productBytes);
    }
    else
    {
        id = bytesToInt<uint32_t>(idBytes);
    }

    debug("Check ID cmd {CMD}: Got={ID}, Expected={EXP}", "CMD", lg2::hex,
          static_cast<uint8_t>(idCmd), "ID", lg2::hex, id, "EXP", lg2::hex,
          expected);

    co_return id == expected;
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

    tbuf = buildByteVector(PMBusCmd::statusCML);
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

    tbuf = buildByteVector(PMBusCmd::storeUserCode);
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
    uint32_t deviceCRC{0};
    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    bool getCRCSuccess = co_await getCRC(&deviceCRC);
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    if (!getCRCSuccess)
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

    tbuf = buildByteVector(PMBusCmd::retoreUserAll);
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
    if (!co_await checkId(PMBusCmd::mfrId, configuration->vendorId))
    {
        co_return false;
    }

    if (!co_await checkId(PMBusCmd::mfrModel, configuration->productId))
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
