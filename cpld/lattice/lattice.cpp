#include "lattice.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <map>
#include <numeric>
#include <vector>

namespace phosphor::software::cpld
{

constexpr uint8_t busyWaitmaxRetry = 30;
constexpr uint8_t busyFlagBit = 0x80;

static constexpr std::string_view tagFuseQuantity = "QF";
static constexpr std::string_view tagUserCodeHex = "UH";
static constexpr std::string_view tagCFStart = "L000";
static constexpr std::string_view tagData = "NOTE TAG DATA";
static constexpr std::string_view tagUserFlashMemory = "NOTE USER MEMORY DATA";
static constexpr std::string_view tagChecksum = "C";
static constexpr std::string_view tagUserCode = "NOTE User Electronic";
static constexpr std::string_view tagEbrInitData = "NOTE EBR_INIT DATA";
static constexpr std::string_view tagEndConfig = "NOTE END CONFIG DATA";
static constexpr std::string_view tagDevName = "NOTE DEVICE NAME";

constexpr uint8_t isOK = 0;
constexpr uint8_t isReady = 0;
constexpr uint8_t busyOrReadyBit = 4;
constexpr uint8_t failOrOKBit = 5;

static uint8_t reverse_bit(uint8_t b)
{
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

std::string LatticeBaseCPLD::uint32ToHexStr(uint32_t value)
{
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(8) << std::hex << std::uppercase
        << value;
    return oss.str();
}

sdbusplus::async::task<bool> LatticeBaseCPLD::updateFirmware(
    const uint8_t* image, size_t imageSize,
    std::function<bool(int)> progressCallBack)
{
    if (progressCallBack == nullptr)
    {
        lg2::error("Error: progressCallBack is null.");
        co_return false;
    }

    if (!image || imageSize == 0)
    {
        lg2::error("Error: image is null.");
        co_return false;
    }

    lg2::debug("CPLD image size: {IMAGESIZE}", "IMAGESIZE", imageSize);
    auto result = co_await prepareUpdate(image, imageSize);
    if (!result)
    {
        lg2::error("Prepare update failed.");
        co_return false;
    }
    lg2::debug("Prepare update success");
    progressCallBack(50);

    result = co_await doUpdate();
    if (!result)
    {
        lg2::error("Do update failed.");
        co_return false;
    }
    lg2::debug("Do update success");
    progressCallBack(90);

    result = co_await finishUpdate();
    if (!result)
    {
        lg2::error("Finish update failed.");
        co_return false;
    }
    lg2::debug("Finish update success");
    progressCallBack(100);

    co_return true;
}

bool LatticeBaseCPLD::jedFileParser(const uint8_t* image, size_t imageSize)
{
    enum class ParseState
    {
        none,
        cfg,
        endCfg,
        ufm,
        checksum,
        userCode
    };
    ParseState state = ParseState::none;

    if (image == nullptr || imageSize == 0)
    {
        lg2::error(
            "Error: JED file is empty or not found. Please check the file.");
        return false;
    }

    std::string content(reinterpret_cast<const char*>(image), imageSize);
    std::istringstream iss(content);
    std::string line;

    auto pushPage = [](std::string& line, std::vector<uint8_t>& sector) {
        if (line[0] == '0' || line[0] == '1')
        {
            while (line.size() >= 8)
            {
                try
                {
                    sector.push_back(static_cast<uint8_t>(
                        std::stoi(line.substr(0, 8), 0, 2)));
                    line.erase(0, 8);
                }
                catch (...)
                {
                    break;
                }
            }
        }
    };

    while (getline(iss, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.empty())
        {
            continue;
        }

        if (line.starts_with(tagFuseQuantity))
        {
            ssize_t numberSize = static_cast<ssize_t>(line.find('*')) -
                                 static_cast<ssize_t>(line.find('F')) - 1;
            if (numberSize > 0)
            {
                fwInfo.fuseQuantity = std::stoul(
                    line.substr(tagFuseQuantity.length(), numberSize));
                lg2::debug("fuseQuantity Size = {QFSIZE}", "QFSIZE",
                           fwInfo.fuseQuantity);
            }
        }
        else if (line.starts_with(tagCFStart) ||
                 line.starts_with(tagEbrInitData))
        {
            state = ParseState::cfg;
            continue;
        }
        else if (line.starts_with(tagEndConfig))
        {
            state = ParseState::endCfg;
            continue;
        }
        else if (line.starts_with(tagUserFlashMemory) ||
                 line.starts_with(tagData))
        {
            state = ParseState::ufm;
            continue;
        }
        else if (line.starts_with(tagUserCode))
        {
            state = ParseState::userCode;
            continue;
        }
        else if (line.starts_with(tagChecksum))
        {
            state = ParseState::checksum;
        }
        else if (line.starts_with(tagDevName))
        {
            lg2::debug("{DEVNAME}", "DEVNAME", line);
            if (line.find(chip) == std::string::npos)
            {
                lg2::debug("STOP UPDATING: The image does not match the chip.");
                return -1;
            }
        }

        switch (state)
        {
            case ParseState::cfg:
                pushPage(line, fwInfo.cfgData);
                break;
            case ParseState::endCfg:
                pushPage(line, sumOnly);
                break;
            case ParseState::ufm:
                pushPage(line, fwInfo.ufmData);
                break;
            case ParseState::checksum:
                if (line.size() > 1)
                {
                    state = ParseState::none;
                    ssize_t numberSize =
                        static_cast<ssize_t>(line.find('*')) -
                        static_cast<ssize_t>(line.find('C')) - 1;
                    if (numberSize <= 0)
                    {
                        lg2::debug("Error in parsing checksum");
                        return -1;
                    }
                    static constexpr auto start = tagChecksum.length();
                    std::istringstream iss(line.substr(start, numberSize));
                    iss >> std::hex >> fwInfo.checksum;
                    lg2::debug("Checksum = 0x{CHECKSUM}", "CHECKSUM",
                               fwInfo.checksum);
                }
                break;
            case ParseState::userCode:
                if (line.starts_with(tagUserCodeHex))
                {
                    state = ParseState::none;
                    ssize_t numberSize =
                        static_cast<ssize_t>(line.find('*')) -
                        static_cast<ssize_t>(line.find('H')) - 1;
                    if (numberSize <= 0)
                    {
                        lg2::debug("Error in parsing usercode");
                        return -1;
                    }
                    std::istringstream iss(
                        line.substr(tagUserCodeHex.length(), numberSize));
                    iss >> std::hex >> fwInfo.version;
                    lg2::debug("UserCode = 0x{USERCODE}", "USERCODE",
                               fwInfo.version);
                }
                break;
            default:
                break;
        }
    }

    lg2::debug("CFG Size = {CFGSIZE}", "CFGSIZE", fwInfo.cfgData.size());
    if (!fwInfo.ufmData.empty())
    {
        lg2::debug("userFlashMemory size = {UFMSIZE}", "UFMSIZE",
                   fwInfo.ufmData.size());
    }

    return true;
}

bool LatticeBaseCPLD::verifyChecksum()
{
    uint32_t calculated = 0U;
    auto addByte = [](uint32_t sum, uint8_t byte) {
        return sum + reverse_bit(byte);
    };

    calculated = std::accumulate(fwInfo.cfgData.begin(), fwInfo.cfgData.end(),
                                 calculated, addByte);
    calculated =
        std::accumulate(sumOnly.begin(), sumOnly.end(), calculated, addByte);
    calculated = std::accumulate(fwInfo.ufmData.begin(), fwInfo.ufmData.end(),
                                 calculated, addByte);

    lg2::debug("Calculated checksum = {CALCULATED}", "CALCULATED", lg2::hex,
               calculated);
    lg2::debug("Checksum from JED file = {JEDFILECHECKSUM}", "JEDFILECHECKSUM",
               lg2::hex, fwInfo.checksum);

    if (fwInfo.checksum != (calculated & 0xFFFF))
    {
        lg2::error("JED file checksum compare fail, "
                   "Calculated checksum = {CALCULATED}, "
                   "Checksum from JED file = {JEDFILECHECKSUM}",
                   "CALCULATED", lg2::hex, calculated, "JEDFILECHECKSUM",
                   lg2::hex, fwInfo.checksum);
        return false;
    }

    lg2::debug("JED file checksum compare success");
    return true;
}

sdbusplus::async::task<bool> LatticeBaseCPLD::enableProgramMode()
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

sdbusplus::async::task<bool> LatticeBaseCPLD::resetConfigFlash()
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

sdbusplus::async::task<bool> LatticeBaseCPLD::programDone()
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

sdbusplus::async::task<bool> LatticeBaseCPLD::disableConfigInterface()
{
    std::vector<uint8_t> request = {commandDisableConfigInterface, 0x0, 0x0};
    std::vector<uint8_t> response;
    co_return i2cInterface.sendReceive(request, response);
}

sdbusplus::async::task<bool> LatticeBaseCPLD::waitBusyAndVerify()
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

sdbusplus::async::task<bool> LatticeBaseCPLD::readBusyFlag(uint8_t& busyFlag)
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

sdbusplus::async::task<bool> LatticeBaseCPLD::readStatusReg(uint8_t& statusReg)
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

sdbusplus::async::task<bool> LatticeBaseCPLD::getVersion(std::string& version)
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

} // namespace phosphor::software::cpld
