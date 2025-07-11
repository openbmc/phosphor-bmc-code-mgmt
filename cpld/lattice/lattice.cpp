#include "lattice.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <fstream>
#include <map>
#include <thread>
#include <vector>

constexpr uint8_t busyWaitmaxRetry = 30;
constexpr uint8_t busyFlagBit = 0x80;
constexpr std::chrono::milliseconds waitBusyTime(200);

static constexpr std::string_view tagQF = "QF";
static constexpr std::string_view tagUH = "UH";
static constexpr std::string_view tagCFStart = "L000";
static constexpr std::string_view tagChecksum = "C";
static constexpr std::string_view tagUserCode = "NOTE User Electronic";
static constexpr std::string_view tagEbrInitData = "NOTE EBR_INIT DATA";

constexpr uint8_t isOK = 0;
constexpr uint8_t isReady = 0;
constexpr uint8_t busyOrReadyBit = 4;
constexpr uint8_t failOrOKBit = 5;

constexpr bool enableUpdateEbrInit = false;

enum cpldI2cCmd
{
    commandEraseFlash = 0x0E,
    commandDisableConfigInterface = 0x26,
    commandReadStatusReg = 0x3C,
    commandResetConfigFlash = 0x46,
    commandProgramDone = 0x5E,
    commandProgramPage = 0x70,
    commandEnableConfigMode = 0x74,
    commandReadFwVersion = 0xC0,
    commandProgramUserCode = 0xC2,
    commandReadDeviceId = 0xE0,
    commandReadBusyFlag = 0xF0,
};

static uint8_t reverse_bit(uint8_t b)
{
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

static int findNumberSize(const std::string& end, const std::string& start,
                          const std::string& line)
{
    auto pos1 = line.find(start);
    auto pos2 = line.find(end);

    if (pos1 == std::string::npos || pos2 == std::string::npos || pos1 >= pos2)
    {
        return false;
    }

    return static_cast<int>(pos2 - pos1 - 1);
}

std::string uint32ToHexStr(uint32_t value)
{
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(8) << std::hex << std::uppercase
        << value;
    return oss.str();
}

bool CpldLatticeManager::jedFileParser()
{
    bool cfStart = false;
    bool ufmStart = false; // for isLCMXO3D
    bool ufmPrepare = false;
    bool versionStart = false;
    bool checksumStart = false;
    bool ebrInitDataStart = false;
    int numberSize = 0;

    if (image == nullptr || imageSize == 0)
    {
        lg2::error(
            "Error: JED file is empty or not found. Please check the file.");
        return false;
    }

    // Convert binary data to a string
    std::string content(reinterpret_cast<const char*>(image), imageSize);
    // Use stringstream to simulate file reading
    std::istringstream iss(content);
    std::string line;

    // Parsing JED file
    while (getline(iss, line))
    {
        if (line.rfind(tagQF, 0) == 0)
        {
            numberSize = findNumberSize("*", "F", line);
            if (numberSize <= 0)
            {
                lg2::error("Error in parsing QF tag");
                return false;
            }
            static constexpr auto start = tagQF.length();
            fwInfo.QF = std::stoul(line.substr(start, numberSize));

            lg2::debug("QF Size = {QF}", "QF", fwInfo.QF);
        }
        else if (line.rfind(tagCFStart, 0) == 0)
        {
            cfStart = true;
        }
        else if (enableUpdateEbrInit && line.rfind(tagEbrInitData, 0) == 0)
        {
            ebrInitDataStart = true;
        }
        else if (ufmPrepare)
        {
            ufmPrepare = false;
            ufmStart = true;
            continue;
        }
        else if (line.rfind(tagUserCode, 0) == 0)
        {
            versionStart = true;
        }
        else if (line.rfind(tagChecksum, 0) == 0)
        {
            checksumStart = true;
        }

        if (line.rfind("NOTE DEVICE NAME:", 0) == 0)
        {
            lg2::error(line.c_str());
            if (line.find(chip) != std::string::npos)
            {
                lg2::debug("[OK] The image device name match with chip name");
            }
            else
            {
                lg2::debug("Abort update as image doesn't match the chip name");
                return false;
            }
        }

        if (cfStart)
        {
            // L000
            if ((line.rfind(tagCFStart, 0)) && (line.size() != 1))
            {
                if ((line.rfind('0', 0) == 0) || (line.rfind('1', 0) == 0))
                {
                    while (!line.empty())
                    {
                        auto binaryStr = line.substr(0, 8);
                        try
                        {
                            fwInfo.cfgData.push_back(
                                std::stoi(binaryStr, 0, 2));
                            line.erase(0, 8);
                        }
                        catch (const std::invalid_argument& error)
                        {
                            break;
                        }
                        catch (...)
                        {
                            lg2::error("Error while parsing CF section");
                            return false;
                        }
                    }
                }
                else
                {
                    lg2::debug("CF size = {CF}", "CF", fwInfo.cfgData.size());
                    cfStart = false;
                    if (!ebrInitDataStart)
                    {
                        ufmPrepare = true;
                    }
                }
            }
        }
        else if (enableUpdateEbrInit && ebrInitDataStart)
        {
            // NOTE EBR_INIT DATA
            if ((line.rfind(tagEbrInitData, 0)) && (line.size() != 1))
            {
                if ((line.rfind('L', 0)) && (line.size() != 1))
                {
                    if ((line.rfind('0', 0) == 0) || (line.rfind('1', 0) == 0))
                    {
                        while (!line.empty())
                        {
                            auto binaryStr = line.substr(0, 8);
                            try
                            {
                                fwInfo.cfgData.push_back(
                                    std::stoi(binaryStr, 0, 2));
                                line.erase(0, 8);
                            }
                            catch (const std::invalid_argument& error)
                            {
                                break;
                            }
                            catch (...)
                            {
                                lg2::error("Error while parsing CF section");
                                return false;
                            }
                        }
                    }
                    else
                    {
                        lg2::debug("CF size with EBR_INIT Data = {CF}", "CF",
                                   fwInfo.cfgData.size());
                        ebrInitDataStart = false;
                        ufmPrepare = true;
                    }
                }
            }
        }
        else if ((checksumStart) && (line.size() != 1))
        {
            checksumStart = false;
            numberSize = findNumberSize("*", "C", line);
            if (numberSize <= 0)
            {
                lg2::error("Error in parsing checksum");
                return false;
            }
            static constexpr auto start = tagChecksum.length();
            std::istringstream iss(line.substr(start, numberSize));
            iss >> std::hex >> fwInfo.checksum;

            lg2::debug("Checksum = {CHECKSUM}", "CHECKSUM", fwInfo.checksum);
        }
        else if (versionStart)
        {
            if ((line.rfind(tagUserCode, 0)) && (line.size() != 1))
            {
                versionStart = false;

                if (line.rfind(tagUH, 0) == 0)
                {
                    numberSize = findNumberSize("*", "H", line);
                    if (numberSize <= 0)
                    {
                        lg2::error("Error in parsing version");
                        return false;
                    }
                    static constexpr auto start = tagUH.length();
                    std::istringstream iss(line.substr(start, numberSize));
                    iss >> std::hex >> fwInfo.version;

                    lg2::debug("UserCode = {USERCODE}", "USERCODE",
                               fwInfo.version);
                }
            }
        }
        else if (ufmStart)
        {
            if ((line.rfind('L', 0)) && (line.size() != 1))
            {
                if ((line.rfind('0', 0) == 0) || (line.rfind('1', 0) == 0))
                {
                    while (!line.empty())
                    {
                        auto binaryStr = line.substr(0, 8);
                        try
                        {
                            fwInfo.ufmData.push_back(
                                std::stoi(binaryStr, 0, 2));
                            line.erase(0, 8);
                        }
                        catch (const std::invalid_argument& error)
                        {
                            break;
                        }
                        catch (...)
                        {
                            lg2::error("Error while parsing UFM section");
                            return false;
                        }
                    }
                }
                else
                {
                    lg2::debug("UFM size = {UFM}", "UFM",
                               fwInfo.ufmData.size());
                    ufmStart = false;
                }
            }
        }
    }

    return true;
}

bool CpldLatticeManager::verifyChecksum()
{
    // Compute check sum
    unsigned int jedFileCheckSum = 0;
    for (unsigned i = 0; i < fwInfo.cfgData.size(); i++)
    {
        jedFileCheckSum += reverse_bit(fwInfo.cfgData.at(i));
    }
    for (unsigned i = 0; i < fwInfo.ufmData.size(); i++)
    {
        jedFileCheckSum += reverse_bit(fwInfo.ufmData.at(i));
    }
    lg2::debug("jedFileCheckSum = {JEDFILECHECKSUM}", "JEDFILECHECKSUM",
               jedFileCheckSum);
    jedFileCheckSum = jedFileCheckSum & 0xffff;

    if ((fwInfo.checksum != jedFileCheckSum) || (fwInfo.checksum == 0))
    {
        lg2::error("CPLD JED File CheckSum Error = {JEDFILECHECKSUM}",
                   "JEDFILECHECKSUM", jedFileCheckSum);
        return false;
    }

    lg2::debug("JED File Checksum compare success");
    return true;
}

sdbusplus::async::task<bool> CpldLatticeManager::readDeviceId()
{
    std::vector<uint8_t> request = {commandReadDeviceId, 0x0, 0x0, 0x0};
    constexpr size_t resSize = 4;
    std::vector<uint8_t> readData(resSize, 0);
    bool success = co_await i2cInterface.sendReceive(
        request.data(), request.size(), readData.data(), resSize);

    if (!success)
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
        chipWantToUpdate->second.deviceId == readData)
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

sdbusplus::async::task<bool> CpldLatticeManager::enableProgramMode()
{
    std::vector<uint8_t> request = {commandEnableConfigMode, 0x08, 0x0, 0x0};
    std::vector<uint8_t> response;

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("Failed to send enable program mode request.");
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

sdbusplus::async::task<bool> CpldLatticeManager::eraseFlash()
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

sdbusplus::async::task<bool> CpldLatticeManager::resetConfigFlash()
{
    std::vector<uint8_t> request;
    std::vector<uint8_t> response;
    if (isLCMXO3D)
    {
        /*
        Set Page Address pointer to the
        beginning of the different internal
        Flash sectors. The bit in YYYY
        defines which sector is selected.
        Bit Flash sector selected
        8 CFG0
        9 CFG1
        10 FEA
        11 PUBKEY
        12 AESKEY
        13 CSEC
        14 UFM0
        15 UFM1
        16 UFM2
        17 UFM3
        18 USEC
        19 Reserved
        20 Reserved
        21 Reserved
        22 Reserved
        commandResetConfigFlash = 0x46, YY YY 00
        */
        if (target.empty() || target == "CFG0")
        {
            request = {commandResetConfigFlash, 0x00, 0x01, 0x00};
        }
        else if (target == "CFG1")
        {
            request = {commandResetConfigFlash, 0x00, 0x02, 0x00};
        }
        else
        {
            lg2::error(
                "Error: unknown target. Only CFG0 and CFG1 are supported.");
            co_return false;
        }
    }
    else
    {
        request = {commandResetConfigFlash, 0x0, 0x0, 0x0};
    }

    co_return i2cInterface.sendReceive(request, response);
}

sdbusplus::async::task<bool> CpldLatticeManager::writeProgramPage()
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

sdbusplus::async::task<bool> CpldLatticeManager::programUserCode()
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

sdbusplus::async::task<bool> CpldLatticeManager::programDone()
{
    std::vector<uint8_t> request = {commandProgramDone, 0x0, 0x0, 0x0};
    std::vector<uint8_t> response;

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("Failed to send program done request.");
        co_return false;
    }

    if (!(co_await waitBusyAndVerify()))
    {
        lg2::error("Wait busy and verify fail");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> CpldLatticeManager::disableConfigInterface()
{
    std::vector<uint8_t> request = {commandDisableConfigInterface, 0x0, 0x0};
    std::vector<uint8_t> response;
    co_return i2cInterface.sendReceive(request, response);
}

sdbusplus::async::task<bool> CpldLatticeManager::waitBusyAndVerify()
{
    uint8_t retry = 0;

    while (retry <= busyWaitmaxRetry)
    {
        uint8_t busyFlag = 0xff;

        auto readBusyFlagResult = co_await readBusyFlag(busyFlag);
        if (!readBusyFlagResult)
        {
            lg2::error("Fail to read busy flag.");
            co_return false;
        }

        if (busyFlag & busyFlagBit)
        {
            co_await sdbusplus::async::sleep_for(ctx, waitBusyTime);
            retry++;
            if (retry > busyWaitmaxRetry)
            {
                lg2::error(
                    "Status Reg : Busy! Please check the I2C bus and address.");
                co_return false;
            }
        }
        else
        {
            break;
        }
    } // while loop busy check

    // Check out status reg
    auto statusReg = std::make_unique<uint8_t>(0xff);

    if (!(co_await readStatusReg(*statusReg)))
    {
        lg2::error("Fail to read status register.");
        co_return false;
    }

    if (((*statusReg >> busyOrReadyBit) & 1) == isReady &&
        ((*statusReg >> failOrOKBit) & 1) == isOK)
    {
        lg2::debug("Status Reg : OK");
        co_return true;
    }

    lg2::error("Status Reg : Fail! Please check the I2C bus and address.");
    co_return false;
}

sdbusplus::async::task<bool> CpldLatticeManager::readBusyFlag(uint8_t& busyFlag)
{
    constexpr size_t resSize = 1;
    std::vector<uint8_t> request = {commandReadBusyFlag, 0x0, 0x0, 0x0};
    std::vector<uint8_t> response(resSize, 0);

    auto success = i2cInterface.sendReceive(request, response);
    if (!success && response.size() != resSize)
    {
        co_return false;
    }
    busyFlag = response.at(0);
    co_return true;
}

sdbusplus::async::task<bool> CpldLatticeManager::readStatusReg(
    uint8_t& statusReg)
{
    std::vector<uint8_t> request = {commandReadStatusReg, 0x0, 0x0, 0x0};
    std::vector<uint8_t> response(4, 0);

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("Failed to send read status register request.");
        co_return false;
    }
    /*
    Read Status Register
    [LSC_READ_STATUS]
    0x3C 00 00 00 N/A YY YY YY YY Bit 1 0
    12 Busy Ready
    13 Fail OK
    */
    statusReg = response.at(2);
    co_return true;
}

sdbusplus::async::task<bool> CpldLatticeManager::readUserCode(
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

sdbusplus::async::task<bool> CpldLatticeManager::XO2XO3FamilyUpdate(
    std::function<bool(int)> progressCallBack)
{
    if (progressCallBack == nullptr)
    {
        lg2::error("Error: progressCallBack is null.");
        co_return false;
    }

    if (!(co_await readDeviceId()))
    {
        co_return false;
    }
    progressCallBack(10);

    if (!jedFileParser())
    {
        lg2::error("JED file parsing failed");
        co_return false;
    }
    progressCallBack(15);

    if (!verifyChecksum())
    {
        lg2::error("Checksum verification failed");
        co_return false;
    }
    progressCallBack(20);

    if (!isLCMXO3D)
    {
        lg2::error("is not LCMXO3D");
    }

    lg2::debug("Starts to update ...");
    lg2::debug("Enable program mode.");
    progressCallBack(25);

    co_await waitBusyAndVerify();

    if (!(co_await enableProgramMode()))
    {
        lg2::error("Enable program mode failed.");
        co_return false;
    }
    progressCallBack(30);

    lg2::debug("Erase flash.");
    if (!(co_await eraseFlash()))
    {
        lg2::error("Erase flash failed.");
        co_return false;
    }
    progressCallBack(40);

    lg2::debug("Reset config flash.");
    if (!(co_await resetConfigFlash()))
    {
        lg2::error("Reset config flash failed.");
        co_return false;
    }
    progressCallBack(50);

    lg2::debug("Write program page ...");
    if (!(co_await writeProgramPage()))
    {
        lg2::error("Write program page failed.");
        co_return false;
    }
    lg2::debug("Write program page done.");
    progressCallBack(60);

    lg2::debug("Program user code.");
    if (!(co_await programUserCode()))
    {
        lg2::error("Program user code failed.");
        co_return false;
    }
    progressCallBack(70);

    if (!(co_await programDone()))
    {
        lg2::error("Program not done.");
        co_return false;
    }
    progressCallBack(80);

    lg2::debug("Disable config interface.");
    if (!(co_await disableConfigInterface()))
    {
        lg2::error("Disable Config Interface failed.");
        co_return false;
    }
    progressCallBack(90);

    lg2::debug("Update completed!");

    co_return true;
}

sdbusplus::async::task<bool> CpldLatticeManager::updateFirmware(
    std::function<bool(int)> progressCallBack)
{
    co_return co_await XO2XO3FamilyUpdate(progressCallBack);
}

sdbusplus::async::task<bool> CpldLatticeManager::getVersion(
    std::string& version)
{
    auto userCode = std::make_unique<uint32_t>(0);

    if (target.empty())
    {
        if (!(co_await readUserCode(*userCode)))
        {
            lg2::error("Read usercode failed.");
            co_return false;
        }

        lg2::debug("CPLD version: {VERSION}", "VERSION", *userCode);
    }
    else if (target == "CFG0" || target == "CFG1")
    {
        isLCMXO3D = true;
        co_await waitBusyAndVerify();

        if (!(co_await enableProgramMode()))
        {
            lg2::error("Enable program mode failed.");
            co_return false;
        }

        if (!(co_await resetConfigFlash()))
        {
            lg2::error("Reset config flash failed.");
            co_return false;
        }

        if (!(co_await readUserCode(*userCode)))
        {
            lg2::error("Read usercode failed.");
            co_return false;
        }

        if (!(co_await programDone()))
        {
            lg2::error("Program not done.");
            co_return false;
        }

        if (!(co_await disableConfigInterface()))
        {
            lg2::error("Disable Config Interface failed.");
            co_return false;
        }

        lg2::debug("CPLD {TARGET} version: {VERSION}", "TARGET", target,
                   "VERSION", *userCode);
    }
    else
    {
        lg2::error("Error: unknown target.");
        co_return false;
    }

    if (*userCode == 0)
    {
        lg2::error("User code is zero, cannot get version.");
        co_return false;
    }
    version = uint32ToHexStr(*userCode);
    co_return true;
}
