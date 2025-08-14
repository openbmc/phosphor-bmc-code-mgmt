#include "xo2xo3.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>
#include <vector>

sdbusplus::async::task<bool> XO2XO3FamilyUpdate::readDeviceId()
{
    std::vector<uint8_t> request = {commandReadDeviceId, 0x0, 0x0, 0x0};
    std::vector<uint8_t> response = {0, 0, 0, 0};

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error(
            "Fail to read device Id. Please check the I2C bus and address.");
        co_return false;
    }

    auto chipWantToUpdate =
        std::find_if(supportedDeviceMap.begin(), supportedDeviceMap.end(),
                     [this](const auto& pair) {
                         return pair.second.chipName == this->chip;
                     });

    if (chipWantToUpdate != supportedDeviceMap.end() &&
        chipWantToUpdate->second.deviceId == response)
    {
        if (chip.rfind("LCMXO3D", 0) == 0)
        {
            isLCMXO3D = true;
            if (!target.empty() && target != "CFG0" && target != "CFG1")
            {
                lg2::error("Unknown target. Only CFG0 and CFG1 are supported.");
                co_return false;
            }
        }

        lg2::debug("Device ID match with chip. Chip name: {CHIPNAME}",
                   "CHIPNAME", chip);
        co_return true;
    }

    lg2::error("The device id doesn't match with chip.");
    co_return false;
}

sdbusplus::async::task<bool> XO2XO3FamilyUpdate::eraseFlash()
{
    std::vector<uint8_t> request;
    std::vector<uint8_t> response;

    if (isLCMXO3D)
    {
        /*
        Erase the different internal
        memories. The bit in YYY defines
        which memory is erased in Flash
        access mode.
        Bit 1=Enable
        8 Erase CFG0
        9 Erase CFG1
        10 Erase UFM0
        11 Erase UFM1
        12 Erase UFM2
        13 Erase UFM3
        14 Erase CSEC
        15 Erase USEC
        16 Erase PUBKEY
        17 Erase AESKEY
        18 Erase FEA
        19 Reserved
        commandEraseFlash = 0x0E, 0Y YY 00
        */
        if (target.empty() || target == "CFG0")
        {
            request = {commandEraseFlash, 0x00, 0x01, 0x00};
        }
        else if (target == "CFG1")
        {
            request = {commandEraseFlash, 0x00, 0x02, 0x00};
        }
        else
        {
            lg2::error("Error: unknown target.");
            co_return false;
        }
    }
    else
    {
        request = {commandEraseFlash, 0xC, 0x0, 0x0};
    }

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("Failed to send erase flash request.");
        co_return false;
    }

    if (!(co_await waitBusyAndVerify()))
    {
        lg2::error("Wait busy and verify fail");
        co_return false;
    }
    co_await sdbusplus::async::sleep_for(ctx, waitBusyTime);
    co_return true;
}

sdbusplus::async::task<bool> XO2XO3FamilyUpdate::writeProgramPage()
{
    /*
    Program one NVCM/Flash page. Can be
    used to program the NVCM0/CFG or
    NVCM1/UFM.
    */
    std::vector<uint8_t> request = {commandProgramPage, 0x0, 0x0, 0x01};
    std::vector<uint8_t> response;
    size_t iterSize = 16;

    for (size_t i = 0; i < fwInfo.cfgData.size(); i += iterSize)
    {
        double progressRate =
            ((double(i) / double(fwInfo.cfgData.size())) * 100);
        std::cout << "Update :" << std::fixed << std::dec
                  << std::setprecision(2) << progressRate << "% \r";

        uint8_t len = ((i + iterSize) < fwInfo.cfgData.size())
                          ? iterSize
                          : (fwInfo.cfgData.size() - i);
        std::vector<uint8_t> data = request;

        data.insert(
            data.end(), fwInfo.cfgData.begin() + static_cast<std::ptrdiff_t>(i),
            fwInfo.cfgData.begin() + static_cast<std::ptrdiff_t>(i + len));

        if (!i2cInterface.sendReceive(data, response))
        {
            lg2::error("Failed to send program page request. {CURRENT}",
                       "CURRENT", uint32ToHexStr(i));
            co_return false;
        }

        /*
         Reference spec
         Important! If don't sleep, it will take a long time to update.
        */
        co_await sdbusplus::async::sleep_for(ctx,
                                             std::chrono::microseconds(200));

        if (!(co_await waitBusyAndVerify()))
        {
            lg2::error("Wait busy and verify fail");
            co_return false;
        }

        data.clear();
    }

    co_return true;
}

sdbusplus::async::task<bool> XO2XO3FamilyUpdate::readUserCode(
    uint32_t& userCode)
{
    constexpr size_t resSize = 4;
    std::vector<uint8_t> request = {commandReadFwVersion, 0x0, 0x0, 0x0};
    std::vector<uint8_t> response(resSize, 0);

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("Failed to send read user code request.");
        co_return false;
    }

    for (size_t i = 0; i < resSize; i++)
    {
        userCode |= response.at(i) << ((3 - i) * 8);
    }
    co_return true;
}

sdbusplus::async::task<bool> XO2XO3FamilyUpdate::programUserCode()
{
    std::vector<uint8_t> request = {commandProgramUserCode, 0x0, 0x0, 0x0};
    std::vector<uint8_t> response;
    for (int i = 3; i >= 0; i--)
    {
        request.push_back((fwInfo.version >> (i * 8)) & 0xFF);
    }

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("Failed to send program user code request.");
        co_return false;
    }
    if (!(co_await waitBusyAndVerify()))
    {
        lg2::error("Wait busy and verify fail");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> XO2XO3FamilyUpdate::prepareUpdate(
    const uint8_t* image, size_t imageSize)
{
    if (!(co_await readDeviceId()))
    {
        co_return false;
    }

    if (!jedFileParser(image, imageSize))
    {
        lg2::error("JED file parsing failed");
        co_return false;
    }
    lg2::debug("JED file parsing success");

    if (!verifyChecksum())
    {
        lg2::error("Checksum verification failed");
        co_return false;
    }

    if (!isLCMXO3D)
    {
        lg2::error("is not LCMXO3D");
    }

    co_return true;
}

sdbusplus::async::task<bool> XO2XO3FamilyUpdate::doUpdate()
{
    co_await waitBusyAndVerify();

    if (!(co_await enableProgramMode()))
    {
        lg2::error("Enable program mode failed.");
        co_return false;
    }

    if (!(co_await eraseFlash()))
    {
        lg2::error("Erase flash failed.");
        co_return false;
    }

    if (!(co_await resetConfigFlash()))
    {
        lg2::error("Reset config flash failed.");
        co_return false;
    }

    if (!(co_await writeProgramPage()))
    {
        lg2::error("Write program page failed.");
        co_return false;
    }

    if (!(co_await programUserCode()))
    {
        lg2::error("Program user code failed.");
        co_return false;
    }

    if (!(co_await programDone()))
    {
        lg2::error("Program not done.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> XO2XO3FamilyUpdate::finishUpdate()
{
    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch)
    if (!(co_await disableConfigInterface()))
    {
        lg2::error("Disable Config Interface failed.");
        co_return false;
    }

    co_return true;
}
