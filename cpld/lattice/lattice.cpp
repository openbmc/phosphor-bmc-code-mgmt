#include "lattice.hpp"

#include <fstream>
#include <map>
#include <thread>
#include <vector>

static uint8_t reverse_bit(uint8_t b)
{
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

const std::map<std::string, std::vector<uint8_t>> xo2xo3DeviceId_t = {
    {"LCMXO3LF-4300", {0x61, 0x2b, 0xc0, 0x43}},
    {"LCMXO3LF-6900", {0x61, 0x2b, 0xd0, 0x43}},
    {"LCMXO3D-4300", {0x01, 0x2e, 0x20, 0x43}},
    {"LCMXO3D-9400", {0x21, 0x2e, 0x30, 0x43}},
};

static int findNumberSize(const std::string& end, const std::string& start,
                          const std::string& line)
{
    auto pos1 = line.find(start);
    auto pos2 = line.find(end);

    if (pos1 == std::string::npos || pos2 == std::string::npos || pos1 >= pos2)
    {
        return -1;
    }

    return static_cast<int>(pos2 - pos1 - 1);
}

int CpldLatticeManager::jedFileParser()
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
        std::cerr << "Invalid JED data" << std::endl;
        return -1;
    }

    // Convert binary data to a string
    std::string content(reinterpret_cast<const char*>(image), imageSize);
    // Use stringstream to simulate file reading
    std::istringstream iss(content);
    std::string line;

    // Parsing JED file
    while (getline(iss, line))
    {
        if (line.rfind(TAG_QF, 0) == 0)
        {
            numberSize = findNumberSize("*", "F", line);
            if (numberSize <= 0)
            {
                std::cerr << "Error in parsing QF tag" << std::endl;
                return -1;
            }
            static constexpr auto start = TAG_QF.length();
            fwInfo.QF = std::stoul(line.substr(start, numberSize));

            std::cout << "QF Size = " << fwInfo.QF << std::endl;
        }
        else if (line.rfind(TAG_CF_START, 0) == 0)
        {
            cfStart = true;
        }
#ifdef ENABLE_UPDATE_EBR_INIT
        else if (line.rfind(TAG_EBR_INIT_DATA, 0) == 0)
        {
            ebrInitDataStart = true;
        }
#endif
        else if (ufmPrepare)
        {
            ufmPrepare = false;
            ufmStart = true;
            continue;
        }
        else if (line.rfind(TAG_USERCODE, 0) == 0)
        {
            versionStart = true;
        }
        else if (line.rfind(TAG_CHECKSUM, 0) == 0)
        {
            checksumStart = true;
        }

        if (line.rfind("NOTE DEVICE NAME:", 0) == 0)
        {
            std::cerr << line << "\n";
            if (line.find(chip) != std::string::npos)
            {
                std::cout
                    << "[OK] The image device name match with chip name\n";
            }
            else
            {
                std::cerr << "STOP UPDATEING: The image not match with chip.\n";
                return -1;
            }
        }

        if (cfStart)
        {
            // L000
            if ((line.rfind(TAG_CF_START, 0)) && (line.size() != 1))
            {
                if ((line.rfind('0', 0) == 0) || (line.rfind('1', 0) == 0))
                {
                    while (!line.empty())
                    {
                        auto binary_str = line.substr(0, 8);
                        try
                        {
                            fwInfo.cfgData.push_back(
                                std::stoi(binary_str, 0, 2));
                            line.erase(0, 8);
                        }
                        catch (const std::invalid_argument& error)
                        {
                            break;
                        }
                        catch (...)
                        {
                            std::cerr << "Error while parsing CF section"
                                      << std::endl;
                            return -1;
                        }
                    }
                }
                else
                {
                    std::cerr
                        << "CF Size = " << fwInfo.cfgData.size() << std::endl;
                    cfStart = false;
                    if (!ebrInitDataStart)
                    {
                        ufmPrepare = true;
                    }
                }
            }
        }
#ifdef ENABLE_UPDATE_EBR_INIT
        else if (ebrInitDataStart)
        {
            // NOTE EBR_INIT DATA
            if ((line.rfind(TAG_EBR_INIT_DATA, 0)) && (line.size() != 1))
            {
                if ((line.rfind('L', 0)) && (line.size() != 1))
                {
                    if ((line.rfind('0', 0) == 0) || (line.rfind('1', 0) == 0))
                    {
                        while (!line.empty())
                        {
                            auto binary_str = line.substr(0, 8);
                            try
                            {
                                fwInfo.cfgData.push_back(
                                    std::stoi(binary_str, 0, 2));
                                line.erase(0, 8);
                            }
                            catch (const std::invalid_argument& error)
                            {
                                break;
                            }
                            catch (...)
                            {
                                std::cerr << "Error while parsing CF section"
                                          << std::endl;
                                return -1;
                            }
                        }
                    }
                    else
                    {
                        std::cerr << "CF Size with ERB_INIT Data = "
                                  << fwInfo.cfgData.size() << std::endl;
                        ebrInitDataStart = false;
                        ufmPrepare = true;
                    }
                }
            }
        }
#endif
        else if ((checksumStart) && (line.size() != 1))
        {
            checksumStart = false;
            numberSize = findNumberSize("*", "C", line);
            if (numberSize <= 0)
            {
                std::cerr << "Error in parsing checksum" << std::endl;
                return -1;
            }
            static constexpr auto start = TAG_CHECKSUM.length();
            std::istringstream iss(line.substr(start, numberSize));
            iss >> std::hex >> fwInfo.CheckSum;

            std::cout << "Checksum = 0x" << std::hex << fwInfo.CheckSum
                      << std::endl;
        }
        else if (versionStart)
        {
            if ((line.rfind(TAG_USERCODE, 0)) && (line.size() != 1))
            {
                versionStart = false;

                if (line.rfind(TAG_UH, 0) == 0)
                {
                    numberSize = findNumberSize("*", "H", line);
                    if (numberSize <= 0)
                    {
                        std::cerr << "Error in parsing version" << std::endl;
                        return -1;
                    }
                    static constexpr auto start = TAG_UH.length();
                    std::istringstream iss(line.substr(start, numberSize));
                    iss >> std::hex >> fwInfo.Version;

                    std::cout << "UserCode = 0x" << std::hex << fwInfo.Version
                              << std::endl;
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
                        auto binary_str = line.substr(0, 8);
                        try
                        {
                            fwInfo.ufmData.push_back(
                                std::stoi(binary_str, 0, 2));
                            line.erase(0, 8);
                        }
                        catch (const std::invalid_argument& error)
                        {
                            break;
                        }
                        catch (...)
                        {
                            std::cerr << "Error while parsing UFM section"
                                      << std::endl;
                            return -1;
                        }
                    }
                }
                else
                {
                    std::cout
                        << "UFM size = " << fwInfo.ufmData.size() << std::endl;
                    ufmStart = false;
                }
            }
        }
    }

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
    std::cout << "jedFileCheckSum = " << jedFileCheckSum << "\n";
    jedFileCheckSum = jedFileCheckSum & 0xffff;

    if ((fwInfo.CheckSum != jedFileCheckSum) || (fwInfo.CheckSum == 0))
    {
        std::cerr << "CPLD JED File CheckSum Error - " << std::hex
                  << jedFileCheckSum << std::endl;
        return -1;
    }
    std::cout << "JED File CheckSum = 0x" << std::hex << jedFileCheckSum
              << std::endl;

    return 0;
}
// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CpldLatticeManager::readDeviceId()
// NOLINTEND(readability-static-accessed-through-instance)
{
    // 0xE0
    std::vector<uint8_t> cmd = {CMD_READ_DEVICE_ID, 0x0, 0x0, 0x0};
    constexpr size_t resSize = 4;
    std::vector<uint8_t> readData(resSize, 0);

    // int ret = i2cWriteReadCmd(cmd, resSize, readData);
    if (!(co_await this->i2cInterface.sendReceive(cmd, resSize, readData)))
    {
        std::cout << "Fail to read device Id." << std::endl;
        co_return false;
    }

    std::cout << "Device ID = ";
    for (auto v : readData)
    {
        std::cout << std::hex << std::setfill('0') << std::setw(2)
                  << unsigned(v) << " ";
    }

    auto chipWantToUpdate = xo2xo3DeviceId_t.find(chip);

    if (chipWantToUpdate != xo2xo3DeviceId_t.end() &&
        chipWantToUpdate->second == readData)
    {
        if (chip.rfind("LCMXO3D", 0) == 0)
        {
            isLCMXO3D = true;
            if (!target.empty() && target != "CFG0" && target != "CFG1")
            {
                std::cerr << "Error: unknown target." << std::endl;
                co_return false;
            }
        }

        std::cout << "[OK] Device ID match with chip\n";
        co_return true;
    }

    std::cerr << "ERROR: The device id not match with chip.\n";
    std::cerr << "Only the following chip names are supported: \n";
    for (const auto& chip : xo2xo3DeviceId_t)
    {
        std::cerr << chip.first << "\n";
    }
    co_return false;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CpldLatticeManager::enableProgramMode()
// NOLINTEND(readability-static-accessed-through-instance)
{
    // 0x74 transparent mode
    std::vector<uint8_t> cmd = {CMD_ENABLE_CONFIG_MODE, 0x08, 0x0, 0x0};
    std::vector<uint8_t> read;

    if (!(co_await this->i2cInterface.sendReceive(cmd, 0, read)))
    {
        co_return false;
    }

    if (!(co_await waitBusyAndVerify()))
    {
        std::cerr << "Wait busy and verify fail" << std::endl;
        co_return false;
    }
    std::this_thread::sleep_for(waitBusyTime);
    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CpldLatticeManager::eraseFlash()
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::vector<uint8_t> cmd;
    std::vector<uint8_t> read;

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
        CMD_ERASE_FLASH = 0x0E, 0Y YY 00
        */
        if (target.empty() || target == "CFG0")
        {
            cmd = {CMD_ERASE_FLASH, 0x00, 0x01, 0x00};
        }
        else if (target == "CFG1")
        {
            cmd = {CMD_ERASE_FLASH, 0x00, 0x02, 0x00};
        }
        else
        {
            std::cerr << "Error: unknown target." << std::endl;
            co_return false;
        }
    }
    else
    {
        cmd = {CMD_ERASE_FLASH, 0xC, 0x0, 0x0};
    }

    // int ret = i2cWriteReadCmd(cmd, 0, read);
    if (!(co_await this->i2cInterface.sendReceive(cmd, 0, read)))
    {
        co_return false;
    }

    if (!(co_await waitBusyAndVerify()))
    {
        std::cerr << "Wait busy and verify fail" << std::endl;
        co_return false;
    }
    std::this_thread::sleep_for(waitBusyTime);
    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CpldLatticeManager::resetConfigFlash()
// NOLINTEND(readability-static-accessed-through-instance)
{
    // CMD_RESET_CONFIG_FLASH = 0x46

    std::vector<uint8_t> cmd;
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
        CMD_RESET_CONFIG_FLASH = 0x46, YY YY 00
        */
        if (target.empty() || target == "CFG0")
        {
            cmd = {CMD_RESET_CONFIG_FLASH, 0x00, 0x01, 0x00};
        }
        else if (target == "CFG1")
        {
            cmd = {CMD_RESET_CONFIG_FLASH, 0x00, 0x02, 0x00};
        }
        else
        {
            std::cerr << "Error: unknown target." << std::endl;
            co_return false;
        }
    }
    else
    {
        cmd = {CMD_RESET_CONFIG_FLASH, 0x0, 0x0, 0x0};
    }
    std::vector<uint8_t> read;
    co_return co_await this->i2cInterface.sendReceive(cmd, 0, read);
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CpldLatticeManager::writeProgramPage()
// NOLINTEND(readability-static-accessed-through-instance)
{
    /*
    CMD_PROGRAM_PAGE = 0x70,

    Program one NVCM/Flash page. Can be
    used to program the NVCM0/CFG or
    NVCM1/UFM.

    */
    std::vector<uint8_t> cmd = {CMD_PROGRAM_PAGE, 0x0, 0x0, 0x01};
    std::vector<uint8_t> read;
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
        std::vector<uint8_t> data = cmd;

        data.insert(
            data.end(), fwInfo.cfgData.begin() + static_cast<std::ptrdiff_t>(i),
            fwInfo.cfgData.begin() + static_cast<std::ptrdiff_t>(i + len));

        if (!(co_await this->i2cInterface.sendReceive(data, 0, read)))
        {
            co_return false;
        }

        /*
         Reference spec
         Important! If don't sleep, it will take a long time to update.
        */
        usleep(200);

        if (!(co_await waitBusyAndVerify()))
        {
            std::cerr << "Wait busy and verify fail" << std::endl;
            co_return false;
        }

        data.clear();
    }

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CpldLatticeManager::programUserCode()
// NOLINTEND(readability-static-accessed-through-instance)
{
    /*
    CMD_PROGRAM_USER_CODE = 0xC2,

    Program user code.
    */
    std::vector<uint8_t> cmd = {CMD_PROGRAM_USER_CODE, 0x0, 0x0, 0x0};
    std::vector<uint8_t> read;
    for (int i = 3; i >= 0; i--)
    {
        cmd.push_back((fwInfo.Version >> (i * 8)) & 0xFF);
    }

    if (!(co_await this->i2cInterface.sendReceive(cmd, 0, read)))
    {
        co_return false;
    }

    if (!(co_await waitBusyAndVerify()))
    {
        std::cerr << "Wait busy and verify fail" << std::endl;
        co_return false;
    }

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CpldLatticeManager::programDone()
// NOLINTEND(readability-static-accessed-through-instance)
{
    // CMD_PROGRAM_DONE = 0x5E
    std::vector<uint8_t> cmd = {CMD_PROGRAM_DONE, 0x0, 0x0, 0x0};
    std::vector<uint8_t> read;

    if (!(co_await this->i2cInterface.sendReceive(cmd, 0, read)))
    {
        co_return false;
    }
    if (!(co_await waitBusyAndVerify()))
    {
        std::cerr << "Wait busy and verify fail" << std::endl;
        co_return false;
    }

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CpldLatticeManager::disableConfigInterface()
// NOLINTEND(readability-static-accessed-through-instance)
{
    // CMD_DISABLE_CONFIG_INTERFACE = 0x26,
    std::vector<uint8_t> cmd = {CMD_DISABLE_CONFIG_INTERFACE, 0x0, 0x0};
    std::vector<uint8_t> read;
    co_return co_await this->i2cInterface.sendReceive(cmd, 0, read);
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CpldLatticeManager::waitBusyAndVerify()
// NOLINTEND(readability-static-accessed-through-instance)
{
    uint8_t retry = 0;

    while (retry <= busyWaitmaxRetry)
    {
        uint8_t busyFlag = 0xff;

        if (!(co_await readBusyFlag(busyFlag)))
        {
            std::cerr << "Fail to read busy flag." << std::endl;
            co_return false;
        }

        if (busyFlag & busyFlagBit)
        {
            std::this_thread::sleep_for(waitBusyTime);
            retry++;
            if (retry > busyWaitmaxRetry)
            {
                std::cout << "Status Reg : Busy!" << std::endl;
                co_return false;
            }
        }
        else
        {
            break;
        }
    } // while loop busy check

    // Check out status reg
    uint8_t statusReg = 0xff;

    if (!(co_await readStatusReg(statusReg)))
    {
        std::cerr << "Fail to read status register." << std::endl;
        co_return false;
    }

    if (((statusReg >> busyOrReadyBit) & 1) == isReady &&
        ((statusReg >> failOrOKBit) & 1) == isOK)
    {
        if (debugMode)
        {
            std::cout << "Status Reg : OK" << std::endl;
        }
    }
    else
    {
        std::cerr << "Status Reg : Fail!" << std::endl;
        co_return false;
    }

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CpldLatticeManager::readBusyFlag(uint8_t& busyFlag)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::vector<uint8_t> cmd = {CMD_READ_BUSY_FLAG, 0x0, 0x0, 0x0};
    constexpr size_t resSize = 1;
    std::vector<uint8_t> readData(resSize, 0);

    // !(co_await this->i2cInterface.sendReceive(cmd, 0, read))
    bool ret =
        !(co_await this->i2cInterface.sendReceive(cmd, resSize, readData));
    if (ret || (readData.size() != resSize))
    {
        co_return false;
    }
    else
    {
        busyFlag = readData.at(0);
    }
    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CpldLatticeManager::readStatusReg(
    uint8_t& statusReg)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::vector<uint8_t> cmd = {CMD_READ_STATUS_REG, 0x0, 0x0, 0x0};
    constexpr size_t resSize = 4;
    std::vector<uint8_t> readData(resSize, 0);

    // !(co_await this->i2cInterface.sendReceive(cmd, 0, read))
    bool ret =
        !(co_await this->i2cInterface.sendReceive(cmd, resSize, readData));
    if (ret || (readData.size() != resSize))
    {
        co_return false;
    }
    else
    {
        /*
        Read Status Register
        [LSC_READ_STATUS]
        0x3C 00 00 00 N/A YY YY YY YY Bit 1 0
        12 Busy Ready
        13 Fail OK
         */
        statusReg = readData.at(2);
    }
    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CpldLatticeManager::readUserCode(
    uint32_t& userCode)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::vector<uint8_t> cmd = {CMD_READ_FW_VERSION, 0x0, 0x0, 0x0};
    constexpr size_t resSize = 4;
    std::vector<uint8_t> readData(resSize, 0);

    // int ret = i2cWriteReadCmd(cmd, resSize, readData);
    if (!(co_await this->i2cInterface.sendReceive(cmd, resSize, readData)))
    {
        co_return false;
    }

    for (size_t i = 0; i < resSize; i++)
    {
        userCode |= readData.at(i) << ((3 - i) * 8);
    }
    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CpldLatticeManager::XO2XO3Family_update()
// NOLINTEND(readability-static-accessed-through-instance)
{
    if (!(co_await readDeviceId()))
    {
        co_return false;
    }

    if (jedFileParser() < 0)
    {
        std::cerr << "JED file parsing failed" << std::endl;
        co_return false;
    }

    if (debugMode)
    {
        if (isLCMXO3D)
        {
            std::cerr << "isLCMXO3D\n";
        }
        else
        {
            std::cerr << "is not LCMXO3D\n";
        }
    }

    std::cout << "Starts to update ..." << std::endl;
    std::cout << "Enable program mode." << std::endl;

    co_await waitBusyAndVerify();

    if (!(co_await enableProgramMode()))
    {
        std::cout << "Enable program mode failed." << std::endl;
        co_return false;
    }

    std::cout << "Erase flash." << std::endl;
    if (!(co_await eraseFlash()))
    {
        std::cerr << "Erase flash failed." << std::endl;
        co_return false;
    }

    std::cout << "Reset config flash." << std::endl;
    if (!(co_await resetConfigFlash()))
    {
        std::cerr << "Reset config flash failed." << std::endl;
        co_return false;
    }

    std::cout << "Write Program Page ..." << std::endl;
    if (!(co_await writeProgramPage()))
    {
        std::cerr << "Write program page failed." << std::endl;
        co_return false;
    }
    std::cout << "Write Program Page Done." << std::endl;

    std::cout << "Program user code." << std::endl;
    if (!(co_await programUserCode()))
    {
        std::cerr << "Program user code failed." << std::endl;
        co_return false;
    }

    if (!(co_await programDone()))
    {
        std::cerr << "Program not done." << std::endl;
        co_return false;
    }

    std::cout << "Disable config interface." << std::endl;
    if (!(co_await disableConfigInterface()))
    {
        std::cerr << "Disable Config Interface failed." << std::endl;
        co_return false;
    }

    std::cout << "\nUpdate completed! Please AC." << std::endl;

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> XO5I2CManager::set_page(uint8_t partition_num,
                                                     uint32_t page_num)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::vector<uint8_t> cmd = {XO5_CMD_SET_PAGE, partition_num,
                                static_cast<uint8_t>((page_num >> 16) & 0xFF),
                                static_cast<uint8_t>((page_num >> 8) & 0xFF),
                                static_cast<uint8_t>(page_num & 0xFF)};
    std::vector<uint8_t> read;

    co_return co_await this->i2cInterface.sendReceive(cmd, 0, read);
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> XO5I2CManager::erase_flash(uint8_t size)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::vector<uint8_t> cmd = {XO5_CMD_ERASE_FLASH, size};
    std::vector<uint8_t> read;
    co_return co_await this->i2cInterface.sendReceive(cmd, 0, read);
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> XO5I2CManager::cfg_reset_addr()
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::vector<uint8_t> cmd = {XO5_CMD_CFG_RESET_ADDR};
    std::vector<uint8_t> read;
    co_return co_await this->i2cInterface.sendReceive(cmd, 0, read);
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<std::vector<uint8_t>> XO5I2CManager::cfg_read_page()
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::vector<uint8_t> cmd = {XO5_CMD_CFG_READ_PAGE};
    std::vector<uint8_t> read(256);

    if (co_await this->i2cInterface.sendReceive(cmd, 256, read))
    {
        co_return read;
    }
    else
    {
        co_return {};
    }
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> XO5I2CManager::cfg_write_page(
    std::vector<uint8_t>& /*byte_list*/)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::vector<uint8_t> cmd = {XO5_CMD_CFG_WRITE_PAGE};
    std::vector<uint8_t> read(0);
    co_return co_await this->i2cInterface.sendReceive(cmd, 0, read);
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> XO5I2CManager::programCfgData(uint8_t cfg)
// NOLINTEND(readability-static-accessed-through-instance)
{
    unsigned int data_offset = 0;

    for (int b_idx = 0; b_idx < XO5_CFG_BLOCK_NUM; ++b_idx)
    {
        co_await set_page(cfg, b_idx * XO5_PAGE_NUM);
        co_await erase_flash(XO5_ERASE_BLOCK);

        for (int p_idx = 0; p_idx < XO5_PAGE_NUM; ++p_idx)
        {
            if (data_offset < fwInfo.cfgData.size())
            {
                int remaining_bytes = static_cast<int>(fwInfo.cfgData.size()) -
                                      static_cast<int>(data_offset);
                int bytes_to_write =
                    std::min(static_cast<int>(XO5_PAGE_SIZE), remaining_bytes);
                std::vector<uint8_t> wr_buffer(
                    fwInfo.cfgData.begin() + data_offset,
                    fwInfo.cfgData.begin() + data_offset + bytes_to_write);

                if (!(co_await cfg_write_page(wr_buffer)))
                {
                    std::cerr << "Failed to write page " << p_idx
                              << " of block " << b_idx << std::endl;
                    co_return false;
                }

                data_offset += bytes_to_write;

                float progressRate =
                    (static_cast<float>(data_offset) /
                     static_cast<float>(fwInfo.cfgData.size())) *
                    100;
                std::cout << "Update :" << std::fixed << std::dec
                          << std::setprecision(2) << progressRate << "% \r";

                usleep(200);
            }
            else
            {
                co_return true;
            }
        }
    }
    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> XO5I2CManager::verifyCfgData(uint8_t cfg)
// NOLINTEND(readability-static-accessed-through-instance)
{
    unsigned int data_offset = 0;

    for (int b_idx = 0; b_idx < XO5_CFG_BLOCK_NUM; ++b_idx)
    {
        co_await set_page(cfg, b_idx * XO5_PAGE_NUM);

        for (int p_idx = 0; p_idx < XO5_PAGE_NUM; ++p_idx)
        {
            if (data_offset < fwInfo.cfgData.size())
            {
                std::vector<uint8_t> read_buffer = co_await cfg_read_page();

                if (read_buffer.empty())
                {
                    std::cerr << "Failed to read page " << p_idx << " of block "
                              << b_idx << std::endl;
                    co_return false;
                }

                int remaining_bytes =
                    static_cast<int>(fwInfo.cfgData.size() - data_offset);
                int bytes_to_compare =
                    std::min(static_cast<int>(XO5_PAGE_SIZE), remaining_bytes);

                if (!std::equal(read_buffer.begin(),
                                read_buffer.begin() + bytes_to_compare,
                                fwInfo.cfgData.begin() + data_offset))
                {
                    std::cerr << "Verification failed at page " << p_idx
                              << " of block " << b_idx << std::endl;
                    co_return false;
                }
                data_offset += bytes_to_compare;
            }
            else
            {
                co_return true;
            }
            float progressRate = (static_cast<float>(data_offset) /
                                  static_cast<float>(fwInfo.cfgData.size())) *
                                 100;
            std::cout << "Verify :" << std::fixed << std::dec
                      << std::setprecision(2) << progressRate << "% \r";
            usleep(200);
        }
    }

    std::cout << "Verification successful." << std::endl;
    co_return true;
}

int XO5I2CManager::XO5jedFileParser()
{
    bool cfStart = false;
    bool checksumStart = false;
    bool endofCF = false;
    int numberSize = 0;

    if (image == nullptr || imageSize == 0)
    {
        std::cerr << "Invalid JED data" << std::endl;
        return -1;
    }

    // Convert binary data to a string
    std::string content(reinterpret_cast<const char*>(image), imageSize);
    // Use stringstream to simulate file reading
    std::istringstream iss(content);
    std::string line;

    // Parsing JED file
    while (getline(iss, line))
    {
        if (line.rfind(TAG_QF, 0) == 0)
        {
            numberSize = findNumberSize("*", "F", line);
            if (numberSize <= 0)
            {
                std::cerr << "Error in parsing QF tag" << std::endl;
                return -1;
            }
            static constexpr auto start = TAG_QF.length();
            fwInfo.QF = std::stoul(line.substr(start, numberSize));

            std::cout << "QF Size = " << fwInfo.QF << std::endl;
        }
        else if (line.rfind(TAG_CF_START, 0) == 0)
        {
            cfStart = true;
        }
        else if (line.rfind(TAG_CF_END, 0) == 0)
        {
            endofCF = true;
            cfStart = false;
        }
        else if (line.rfind(TAG_CHECKSUM, 0) == 0)
        {
            checksumStart = true;
        }

        if (line.rfind("NOTE DEVICE NAME:", 0) == 0)
        {
            std::cerr << line << "\n";
            if (line.find(chip) != std::string::npos)
            {
                std::cout
                    << "[OK] The image device name match with chip name\n";
            }
            else
            {
                std::cerr << "STOP UPDATEING: The image not match with chip.\n";
                return -1;
            }
        }

        if (cfStart)
        {
            if ((line.rfind(TAG_CF_START, 0)) && (line.size() != 1))
            {
                if ((line.rfind('0', 0) == 0) || (line.rfind('1', 0) == 0))
                {
                    while (!line.empty())
                    {
                        auto binary_str = line.substr(0, 8);
                        try
                        {
                            fwInfo.cfgData.push_back(
                                std::stoi(binary_str, 0, 2));
                            line.erase(0, 8);
                        }
                        catch (const std::invalid_argument& error)
                        {
                            break;
                        }
                        catch (...)
                        {
                            std::cerr << "Error while parsing CF section"
                                      << std::endl;
                            return -1;
                        }
                    }
                }
                else
                {
                    continue;
                }
            }
        }
        else if (endofCF)
        {
            std::cerr << "CF Size = " << fwInfo.cfgData.size() << std::endl;
            endofCF = false;
        }
        else if ((checksumStart) && (line.size() != 1))
        {
            checksumStart = false;
            numberSize = findNumberSize("*", "C", line);
            if (numberSize <= 0)
            {
                std::cerr << "Error in parsing checksum" << std::endl;
                return -1;
            }
            static constexpr auto start = TAG_CHECKSUM.length();
            std::istringstream iss(line.substr(start, numberSize));
            iss >> std::hex >> fwInfo.CheckSum;

            std::cout << "Checksum = 0x" << std::hex << fwInfo.CheckSum
                      << std::endl;
        }
    }
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
    std::cout << "jedFileCheckSum = " << jedFileCheckSum << "\n";
    jedFileCheckSum = jedFileCheckSum & 0xffff;

    if ((fwInfo.CheckSum != jedFileCheckSum) || (fwInfo.CheckSum == 0))
    {
        std::cerr << "CPLD JED File CheckSum Error - " << std::hex
                  << jedFileCheckSum << std::endl;
        return -1;
    }
    else
    {
        std::cout << "[OK] JED File Checksum compare success" << std::endl;
    }

    return 0;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CpldLatticeManager::XO5Family_update()
// NOLINTEND(readability-static-accessed-through-instance)
{
    uint8_t operate_taget;
    XO5I2CManager i2cManager(bus, addr, image, imageSize, chip, "i2c", "CFG0",
                             false);

    std::cout << "Starting to update " << chip << std::endl;

    if (target.empty() || target == "CFG0")
    {
        operate_taget = XO5I2CManager::XO5_PARTITION_CFG0;
    }
    else if (target == "CFG1")
    {
        operate_taget = XO5I2CManager::XO5_PARTITION_CFG1;
    }
    else
    {
        std::cerr << "Error: unknown target." << std::endl;
        co_return false;
    }

    if (i2cManager.XO5jedFileParser() < 0)
    {
        std::cerr << "JED file parsing failed" << std::endl;
        co_return false;
    }

    std::cout << "Program CFG"
              << (operate_taget == XO5I2CManager::XO5_PARTITION_CFG0 ? "0"
                                                                     : "1")
              << " data ..." << std::endl;
    if (!(co_await i2cManager.programCfgData(operate_taget)))
    {
        std::cerr << "Program cfg data failed." << std::endl;
        co_return false;
    }

    std::cout << "Verify CFG data ..." << std::endl;
    if (!(co_await i2cManager.verifyCfgData(operate_taget)))
    {
        std::cerr << "Verify cfg data failed." << std::endl;
        co_return false;
    }

    std::cout << "\nUpdate completed! Please AC." << std::endl;

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CpldLatticeManager::fwUpdate()
// NOLINTEND(readability-static-accessed-through-instance)
{
    if (xo2xo3DeviceId_t.find(chip) != xo2xo3DeviceId_t.end())
    {
        co_return co_await XO2XO3Family_update();
    }
    else if (chip == "LFMXO5-25")
    {
        co_return co_await XO5Family_update();
    }
    else
    {
        std::cerr << "Unsupported chip type: " << chip << std::endl;
        co_return false;
    }
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CpldLatticeManager::getVersion()
// NOLINTEND(readability-static-accessed-through-instance)
{
    uint32_t userCode = 0;

    if (target.empty())
    {
        if (!(co_await readUserCode(userCode)))
        {
            std::cerr << "Read usercode failed." << std::endl;
            co_return false;
        }

        std::cout << "CPLD version: 0x" << std::hex << std::setfill('0')
                  << std::setw(8) << userCode << std::endl;
    }
    else if (target == "CFG0" || target == "CFG1")
    {
        isLCMXO3D = true;
        co_await waitBusyAndVerify();

        if (!(co_await enableProgramMode()))
        {
            std::cerr << "Enable program mode failed." << std::endl;
            co_return false;
        }

        if (!(co_await resetConfigFlash()))
        {
            std::cerr << "Reset config flash failed." << std::endl;
            co_return false;
        }

        if (!(co_await readUserCode(userCode)))
        {
            std::cerr << "Read usercode failed." << std::endl;
            co_return false;
        }

        if (!(co_await programDone()))
        {
            std::cerr << "Program not done." << std::endl;
            co_return false;
        }

        if (!(co_await disableConfigInterface()))
        {
            std::cerr << "Disable Config Interface failed." << std::endl;
            co_return false;
        }

        std::cout << "CPLD " << target << " version: 0x" << std::hex
                  << std::setfill('0') << std::setw(8) << userCode << std::endl;
    }
    else
    {
        std::cerr << "Error: unknown target." << std::endl;
        co_return false;
    }
    co_return true;
}
