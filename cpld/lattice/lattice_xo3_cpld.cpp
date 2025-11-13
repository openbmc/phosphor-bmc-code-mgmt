#include "lattice_xo3_cpld.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>
#include <vector>

namespace phosphor::software::cpld
{

sdbusplus::async::task<bool> LatticeXO3CPLD::readDeviceId()
{
    std::vector<uint8_t> request = {commandReadDeviceId, 0x0, 0x0, 0x0};
    std::vector<uint8_t> response = {0, 0, 0, 0};

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error(
            "Fail to read device Id. Please check the I2C bus and address.");
        co_return false;
    }

    auto chipWantToUpdate = std::find_if(
        supportedDeviceMap.begin(), supportedDeviceMap.end(),
        [this](const auto& pair) {
            auto chipModel =
                getLatticeChipStr(pair.first, latticeStringType::modelString);
            return chipModel == this->chip;
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

sdbusplus::async::task<bool> LatticeXO3CPLD::eraseFlash()
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

sdbusplus::async::task<bool> LatticeXO3CPLD::writeProgramPage()
{
    /*
    Program one NVCM/Flash page. Can be
    used to program the NVCM0/CFG or
    NVCM1/UFM.
    */
    size_t iterSize = 16;

    for (size_t i = 0; (i * iterSize) < fwInfo.cfgData.size(); i++)
    {
        size_t byteOffset = i * iterSize;
        double progressRate =
            ((double(byteOffset) / double(fwInfo.cfgData.size())) * 100);
        std::cout << "Update :" << std::fixed << std::dec
                  << std::setprecision(2) << progressRate << "% \r";

        uint8_t len = ((byteOffset + iterSize) < fwInfo.cfgData.size())
                          ? iterSize
                          : (fwInfo.cfgData.size() - byteOffset);
        auto pageData = std::vector<uint8_t>(
            fwInfo.cfgData.begin() + static_cast<std::ptrdiff_t>(byteOffset),
            fwInfo.cfgData.begin() +
                static_cast<std::ptrdiff_t>(byteOffset + len));

        size_t retry = 0;
        const size_t maxWriteRetry = 10;
        while (retry < maxWriteRetry)
        {
            if (!(co_await programSinglePage(i, pageData)))
            {
                retry++;
                continue;
            }

            if (!(co_await verifySinglePage(i, pageData)))
            {
                retry++;
                continue;
            }

            break;
        }

        if (retry >= maxWriteRetry)
        {
            lg2::error("Program and verify page failed");
            co_return false;
        }
    }

    if (!(co_await waitBusyAndVerify()))
    {
        lg2::error("Wait busy and verify fail");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO3CPLD::readUserCode(uint32_t& userCode)
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

sdbusplus::async::task<bool> LatticeXO3CPLD::programUserCode()
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

sdbusplus::async::task<bool> LatticeXO3CPLD::prepareUpdate(const uint8_t* image,
                                                           size_t imageSize)
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

sdbusplus::async::task<bool> LatticeXO3CPLD::doUpdate()
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

sdbusplus::async::task<bool> LatticeXO3CPLD::finishUpdate()
{
    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch)
    if (!(co_await disableConfigInterface()))
    {
        lg2::error("Disable Config Interface failed.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO3CPLD::programSinglePage(
    uint16_t pageOffset, std::span<const uint8_t> pageData)
{
    // Set Page Offset
    std::vector<uint8_t> emptyResp(0);
    std::vector<uint8_t> setPageAddrCmd = {
        commandSetPageAddress, 0x0, 0x0, 0x0, 0x00, 0x00, 0x00, 0x00};
    setPageAddrCmd[6] = static_cast<uint8_t>(pageOffset >> 8); // high byte
    setPageAddrCmd[7] = static_cast<uint8_t>(pageOffset);      // low byte

    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch)
    bool success = co_await i2cInterface.sendReceive(
        setPageAddrCmd.data(), setPageAddrCmd.size(), nullptr, 0);
    if (!success)
    {
        lg2::error("Write page address failed");
        co_return false;
    }

    // Write Page Data
    constexpr uint8_t pageCount = 1;
    std::vector<uint8_t> writeCmd = {commandProgramPage, 0x0, 0x0, pageCount};
    writeCmd.insert(writeCmd.end(), pageData.begin(), pageData.end());

    success = co_await i2cInterface.sendReceive(writeCmd.data(),
                                                writeCmd.size(), nullptr, 0);
    if (!success)
    {
        lg2::error("Write page data failed");
        co_return false;
    }

    co_await sdbusplus::async::sleep_for(ctx, std::chrono::microseconds(200));

    if (!(co_await waitBusyAndVerify()))
    {
        lg2::error("Wait busy and verify fail");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO3CPLD::verifySinglePage(
    uint16_t pageOffset, std::span<const uint8_t> pageData)
{
    // Set Page Offset
    std::vector<uint8_t> emptyResp(0);
    std::vector<uint8_t> setPageAddrCmd = {
        commandSetPageAddress, 0x0, 0x0, 0x0, 0x00, 0x00, 0x00, 0x00};
    setPageAddrCmd[6] = static_cast<uint8_t>(pageOffset >> 8); // high byte
    setPageAddrCmd[7] = static_cast<uint8_t>(pageOffset);      // low byte

    if (!i2cInterface.sendReceive(setPageAddrCmd, emptyResp))
    {
        lg2::error("Write page address failed");
        co_return false;
    }

    // Read Page Data
    constexpr uint8_t pageCount = 1;
    std::vector<uint8_t> readData(pageData.size());
    std::vector<uint8_t> readCmd = {commandReadPage, 0x0, 0x0, pageCount};

    if (!i2cInterface.sendReceive(readCmd, readData))
    {
        lg2::error("Read page data failed");
        co_return false;
    }

    constexpr size_t pageSize = 16;
    auto mismatch_pair =
        std::mismatch(pageData.begin(), pageData.end(), readData.begin());
    if (mismatch_pair.first != pageData.end())
    {
        size_t idx = std::distance(pageData.begin(), mismatch_pair.first);
        lg2::error("Verify failed at {INDEX}", "INDEX",
                   ((static_cast<size_t>(pageSize * pageOffset)) + idx));
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO3CPLDJtag::getVersion(
    std::string& version)
{
    uint8_t versionReg = 0xf4;
    uint8_t regLength = 1;
    uint8_t verLength = 3;
    uint8_t readBuf[4] = {0};

    auto success = co_await i2cInterface.sendReceive(&versionReg, 
                                    regLength, readBuf, verLength);
    if (!success)
    {
        lg2::error("Failed to get version");
        co_return success;
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(2) << static_cast<int>(readBuf[0]) << "."
        << std::setw(2) << static_cast<int>(readBuf[1]) << "."
        << std::setw(2) << static_cast<int>(readBuf[2]);

    version = oss.str();

    co_return success;
}

} // namespace phosphor::software::cpld
