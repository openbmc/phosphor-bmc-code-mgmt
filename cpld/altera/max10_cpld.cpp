#include "max10_cpld.hpp"

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/bus.hpp>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <sstream>
#include <thread>

// #define DEBUG
#ifdef DEBUG
#define LG2_DEBUG(fmt, ...)                                                    \
    lg2::info("[{SOURCE_FUNC}] " fmt, "SOURCE_FUNC", std::string(__func__),    \
              ##__VA_ARGS__)
#else
#define LG2_DEBUG(fmt, ...)                                                    \
    do                                                                         \
    {                                                                          \
    } while (0)
#endif

namespace phosphor::software::cpld
{

Max10CPLD::Max10CPLD(sdbusplus::async::context& ctx, const uint16_t bus,
                     const uint8_t address, const std::string& chip,
                     const std::string& target, const bool debugMode,
                     const Max10Profile& profile) :
    ctx_(ctx), bus_(bus), address_(address), chip_(chip), target_(target),
    debugMode_(debugMode), profile_(profile)
{}

Max10CPLD::~Max10CPLD()
{
    closeDevice();
}

bool Max10CPLD::ensureOpen()
{
    if (fd_ >= 0)
        return true;

    std::string path = "/dev/i2c-" + std::to_string(bus_);
    fd_ = open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd_ < 0)
    {
        lg2::error("Failed to open {PATH}: {ERR}", "PATH", path, "ERR",
                   std::strerror(errno));
        return false;
    }

    if (ioctl(fd_, I2C_SLAVE, address_) < 0)
    {
        lg2::error("Failed to set I2C address 0x{ADDR:02X}: {ERR}", "ADDR",
                   address_, "ERR", std::strerror(errno));
        closeDevice();
        return false;
    }
    return true;
}

void Max10CPLD::closeDevice()
{
    if (fd_ >= 0)
    {
        close(fd_);
        fd_ = -1;
    }
}

std::string Max10CPLD::getUpdateGpioNameFromDBus() const
{
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto mapperCall = bus.new_method_call(
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTree");

        mapperCall.append("/xyz/openbmc_project/inventory", 0,
                          std::vector<std::string>{
                              "xyz.openbmc_project.Configuration.Max10Cpld"});

        auto mapperReply = bus.call(mapperCall);
        std::map<std::string, std::map<std::string, std::vector<std::string>>>
            subtree;
        mapperReply.read(subtree);

        for (const auto& [path, services] : subtree)
        {
            for (const auto& [service, interfaces] : services)
            {
                auto getCall = bus.new_method_call(
                    service.c_str(), path.c_str(),
                    "org.freedesktop.DBus.Properties", "GetAll");
                getCall.append("xyz.openbmc_project.Configuration.Max10Cpld");

                auto getReply = bus.call(getCall);
                std::map<std::string, std::variant<uint64_t, std::string>>
                    props;
                getReply.read(props);

                if (props.contains("Bus") && props.contains("Address"))
                {
                    uint64_t cfgBus = std::get<uint64_t>(props.at("Bus"));
                    uint64_t cfgAddr = 0;

                    if (std::holds_alternative<uint64_t>(props.at("Address")))
                    {
                        cfgAddr = std::get<uint64_t>(props.at("Address"));
                    }
                    else if (std::holds_alternative<std::string>(
                                 props.at("Address")))
                    {
                        cfgAddr = std::stoull(
                            std::get<std::string>(props.at("Address")), nullptr,
                            0);
                    }

                    if (cfgBus == bus_ && cfgAddr == address_)
                    {
                        if (props.contains("UpdateGpio"))
                        {
                            return std::get<std::string>(
                                props.at("UpdateGpio"));
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to query UpdateGpio from D-Bus: {ERR}", "ERR",
                   e.what());
    }
    return "";
}

bool Max10CPLD::readReg(uint32_t reg, uint32_t& value)
{
    if (!ensureOpen())
        return false;

    std::array<uint8_t, 4> addrBuf;
    addrBuf[0] = (reg >> 24) & 0xFF;
    addrBuf[1] = (reg >> 16) & 0xFF;
    addrBuf[2] = (reg >> 8) & 0xFF;
    addrBuf[3] = reg & 0xFF;

    std::array<uint8_t, 4> dataBuf{};
    struct i2c_msg msgs[2] = {};

    msgs[0].addr = address_;
    msgs[0].flags = 0;
    msgs[0].len = addrBuf.size();
    msgs[0].buf = addrBuf.data();

    msgs[1].addr = address_;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = dataBuf.size();
    msgs[1].buf = dataBuf.data();

    struct i2c_rdwr_ioctl_data msgset = {};
    msgset.msgs = msgs;
    msgset.nmsgs = 2;

    int retry = 3;
    while (retry--)
    {
        if (ioctl(fd_, I2C_RDWR, &msgset) >= 0)
        {
            if (profile_.littleEndian)
            {
                value = (dataBuf[3] << 24) | (dataBuf[2] << 16) |
                        (dataBuf[1] << 8) | dataBuf[0];
            }
            else
            {
                value = (dataBuf[0] << 24) | (dataBuf[1] << 16) |
                        (dataBuf[2] << 8) | dataBuf[3];
            }
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    char regStr[32];
    snprintf(regStr, sizeof(regStr), "0x%X", reg);
    lg2::error("I2C write reg {REG} failed via ioctl: {ERR}", "REG", regStr,
               "ERR", std::strerror(errno));
    return false;
}

bool Max10CPLD::writeReg(uint32_t reg, uint32_t value)
{
    if (!ensureOpen())
        return false;

    std::array<uint8_t, 8> data{};
    data[0] = (reg >> 24) & 0xFF;
    data[1] = (reg >> 16) & 0xFF;
    data[2] = (reg >> 8) & 0xFF;
    data[3] = reg & 0xFF;

    if (profile_.littleEndian)
    {
        data[4] = value & 0xFF;
        data[5] = (value >> 8) & 0xFF;
        data[6] = (value >> 16) & 0xFF;
        data[7] = (value >> 24) & 0xFF;
    }
    else
    {
        data[4] = (value >> 24) & 0xFF;
        data[5] = (value >> 16) & 0xFF;
        data[6] = (value >> 8) & 0xFF;
        data[7] = value & 0xFF;
    }

    struct i2c_msg msg = {};
    msg.addr = address_;
    msg.flags = 0;
    msg.len = data.size();
    msg.buf = data.data();

    struct i2c_rdwr_ioctl_data msgset = {};
    msgset.msgs = &msg;
    msgset.nmsgs = 1;

    int retry = 3;
    while (retry--)
    {
        if (ioctl(fd_, I2C_RDWR, &msgset) >= 0)
        {
            // Give bridge time to process write
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    char regStr[32];
    snprintf(regStr, sizeof(regStr), "0x%X", reg);
    lg2::error("I2C write reg {REG} failed via ioctl: {ERR}", "REG", regStr,
               "ERR", std::strerror(errno));
    return false;
}

bool Max10CPLD::readStatus(uint32_t& status)
{
    return readReg(profile_.csrBase + 0x00, status);
}

bool Max10CPLD::validateProfile() const
{
    // Removed 0 check for csrBase, dataBase, bootBase as 0x0 is a valid
    // offset in Clemente (e.g. ON_CHIP_FLASH_IP_DATA_REG)

    if (profile_.startAddr >= profile_.endAddr)
    {
        lg2::error("Invalid CFM range for {CHIP}", "CHIP", chip_);
        return false;
    }
    if (profile_.imageType == 0)
    {
        lg2::error("Invalid image type for {CHIP}", "CHIP", chip_);
        return false;
    }
    return true;
}

bool Max10CPLD::protectSectors()
{
    uint32_t ctrl = 0;
    if (!readReg(profile_.csrBase + 0x04, ctrl))
    {
        return false;
    }
    ctrl |= (0x1F << 5);
    return writeReg(profile_.csrBase + 0x04, ctrl);
}

bool Max10CPLD::unprotectSector(int sectorId)
{
    uint32_t ctrl = 0;
    if (!readReg(profile_.csrBase + 0x04, ctrl))
    {
        return false;
    }

    switch (sectorId)
    {
        case 5:
            ctrl &= ~protectSec5;
            break;
        case 4:
            ctrl &= ~protectSec4;
            break;
        case 3:
            ctrl &= ~protectSec3;
            break;
        case 2:
            ctrl &= ~protectSec2;
            break;
        case 1:
            ctrl &= ~protectSec1;
            break;
        case 0:
            ctrl = 0xFFFFFFFF;
            break;
        default:
            lg2::error("Unknown sector id {ID}", "ID", sectorId);
            return false;
    }
    LG2_DEBUG("unprotect {SECTORID} Write {CTRL}", "SECTORID", sectorId, "CTRL",
              toHex8(ctrl));
    if (writeReg(profile_.csrBase + 0x04, ctrl))
    {
        LG2_DEBUG("unprotect sector SUCCESS");
        return true;
    }

    LG2_DEBUG("unprotect sector Fail");
    return false;
}

bool Max10CPLD::eraseSector(int sectorId)
{
    uint32_t ctrl = 0;
    if (!readReg(profile_.csrBase + 0x04, ctrl))
    {
        return false;
    }

    ctrl &= ~(0x7u << 20);

    switch (sectorId)
    {
        case 5:
            ctrl |= sectorErase5;
            break;
        case 4:
            ctrl |= sectorErase4;
            break;
        case 3:
            ctrl |= sectorErase3;
            break;
        case 2:
            ctrl |= sectorErase2;
            break;
        case 1:
            ctrl |= sectorErase1;
            break;
        case 0:
            ctrl |= sectorEraseNone;
            break;
        default:
            lg2::error("Unknown sector id {ID}", "ID", sectorId);
            return false;
    }

    LG2_DEBUG("erase {SECTORID} Write {CTRL}", "SECTORID", sectorId, "CTRL",
              toHex8(ctrl));
    if (!writeReg(profile_.csrBase + 0x04, ctrl))
    {
        LG2_DEBUG("erase sector Fail");
        return false;
    }

    LG2_DEBUG("erase sector SUCCESS");
    return waitEraseDone();
}

bool Max10CPLD::waitEraseDone(int timeoutCount)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for (int cnt = 0; cnt < timeoutCount; ++cnt)
    {
        uint32_t status = 0;
        if (!readStatus(status))
            continue;

        status &= statusMask;

        if (status & statusBusyErase)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            continue;
        }

        if (status & statusEraseSuccess)
        {
            return true;
        }

        if (status != 0)
        {
            lg2::error("Erase failed, status={STATUS}", "STATUS",
                       toHex8(status));
            return false;
        }
    }
    lg2::error("Erase timeout");
    return false;
}

bool Max10CPLD::waitWriteDone(int timeoutCount)
{
    for (int cnt = 0; cnt < timeoutCount; ++cnt)
    {
        uint32_t status = 0;
        if (!readStatus(status))
            continue;

        status &= statusMask;

        if (status & statusBusyWrite)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            continue;
        }

        if (status & statusWriteSuccess)
        {
            return true;
        }

        if (status != 0)
        {
            lg2::error("Write failed, status={STATUS}", "STATUS",
                       toHex8(status));
            return false;
        }
    }
    lg2::error("Write timeout");
    return false;
}

uint8_t Max10CPLD::bitReverse(uint8_t v)
{
    /*Swap LSB with MSB before write into CFM*/
    v = static_cast<uint8_t>((v & 0xF0) >> 4 | (v & 0x0F) << 4);
    v = static_cast<uint8_t>((v & 0xCC) >> 2 | (v & 0x33) << 2);
    v = static_cast<uint8_t>((v & 0xAA) >> 1 | (v & 0x55) << 1);
    return v;
}

uint32_t Max10CPLD::packWord(const uint8_t* p)
{
    return (static_cast<uint32_t>(bitReverse(p[0])) << 24) |
           (static_cast<uint32_t>(bitReverse(p[1])) << 16) |
           (static_cast<uint32_t>(bitReverse(p[2])) << 8) |
           (static_cast<uint32_t>(bitReverse(p[3])) << 0);
}

std::string Max10CPLD::toHex8(uint32_t v)
{
    char buf[11]{};
    std::snprintf(buf, sizeof(buf), "0x%08X", v);
    return buf;
}

bool Max10CPLD::programRpd(const uint8_t* image, size_t imageSize,
                           std::function<bool(int)> progressCallBack)
{
    if (!validateProfile())
        return false;

    LG2_DEBUG("MAX10 Update: imageSize={SIZE}, type={TYPE}", "SIZE", imageSize,
              "TYPE", profile_.imageType);

    const auto expectedSize =
        static_cast<size_t>(profile_.endAddr - profile_.startAddr);
    if (imageSize != expectedSize)
    {
        lg2::error("Unsupported image type {TYPE}", "TYPE", profile_.imageType);
        return false;
    }

    int ret = 0;
    switch (profile_.imageType)
    {
        case 1:
            ret |= unprotectSector(5) ? 0 : 1;
            ret |= eraseSector(5) ? 0 : 1;
            break;
        case 2:
            ret |= unprotectSector(3) ? 0 : 1;
            ret |= unprotectSector(4) ? 0 : 1;
            ret |= eraseSector(3) ? 0 : 1;
            ret |= eraseSector(4) ? 0 : 1;
            break;
        case 3:
            ret |= unprotectSector(4) ? 0 : 1;
            ret |= eraseSector(4) ? 0 : 1;
            break;
        default:
            lg2::error("Unsupported image type {TYPE}", "TYPE",
                       profile_.imageType);
            return false;
    }

    if (ret != 0)
    {
        lg2::error("Erase or unprotect failed");
        return false;
    }

    // Set erase none
    ret |= eraseSector(0);

    if (progressCallBack)
    {
        progressCallBack(10);
    }

    // start program
    size_t offset = 0;
    for (uint32_t addr = profile_.startAddr; addr < profile_.endAddr;
         addr += 4, offset += 4)
    {
        if ((offset + 3) >= imageSize)
        {
            break;
        }

        const uint32_t word = packWord(image + offset);

        /*Command to write into On-Chip Flash IP*/
        if (!writeReg(profile_.dataBase + addr, word))
        {
            return false;
        }
        LG2_DEBUG("write add:{ADDRESS} word:{WORD}", "ADDRESS",
                  toHex8(profile_.dataBase + addr), "WORD", toHex8(word));

        if (!waitWriteDone())
        {
            return false;
        }

        if (progressCallBack && (offset % 1024 == 0))
        {
            const int pct = 10 + static_cast<int>((offset * 85) / imageSize);
            progressCallBack(pct);
        }
    }
    LG2_DEBUG("All write SUCCESS");

    if (!protectSectors())
    {
        return false;
    }

    if (progressCallBack)
    {
        progressCallBack(100);
    }

    return true;
}

sdbusplus::async::task<bool> Max10CPLD::updateFirmware(
    bool force, const uint8_t* image, size_t imageSize,
    std::function<bool(int)> progressCallBack)
{
    (void)force;

    if (image == nullptr || imageSize == 0)
    {
        lg2::error("MAX10 image is empty");
        co_return false;
    }

    if (!ensureOpen())
    {
        co_return false;
    }

    std::string gpioName = getUpdateGpioNameFromDBus();
    gpiod::line updateGpio;

    if (!gpioName.empty())
    {
        updateGpio = gpiod::find_line(gpioName);
        LG2_DEBUG("Enable GPIO name {GPIO}", "GPIO", gpioName);
        if (!updateGpio)
        {
            lg2::error("Can not find GPIO line: {GPIO}", "GPIO", gpioName);
            co_return false;
        }

        if (updateGpio)
        {
            try
            {
                // Active High: 1 is Enable
                updateGpio.request(
                    {"max10-update", gpiod::line_request::DIRECTION_OUTPUT, 0},
                    1);
                LG2_DEBUG("GPIO {GPIO} has been set high to enable update mode",
                          "GPIO", gpioName);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                int readback = updateGpio.get_value();
                LG2_DEBUG(
                    "After requesting GPIO {GPIO}, the readback value is {VAL}",
                    "GPIO", gpioName, "VAL", readback);

                if (readback != 1)
                {
                    lg2::error(
                        "GPIO {GPIO} readback does not match the expected value: expect={EXP}, got={VAL}",
                        "GPIO", gpioName, "EXP", 1, "VAL", readback);
                    co_return false;
                }
            }
            catch (const std::exception& e)
            {
                lg2::error("GPIO request failed: {ERR}", "ERR", e.what());
                co_return false;
            }
        }
    }

    bool result = programRpd(image, imageSize, std::move(progressCallBack));

    if (updateGpio)
    {
        updateGpio.set_value(0); // Active Low: Disable
        updateGpio.release();
        LG2_DEBUG("GPIO {GPIO} has been set low to exit update mode", "GPIO",
                  gpioName);
    }

    co_return result;
}

sdbusplus::async::task<bool> Max10CPLD::getVersion(std::string& version)
{
    if (!ensureOpen())
    {
        co_return false;
    }

    uint32_t ver = 0;
    if (!readReg(versionReg, ver))
    {
        co_return false;
    }

    // Align with previous manually-tested endianness swap logic
    ver = ((0x000000ff & ver) << 24) | ((0x0000ff00 & ver) << 8) |
          ((0x00ff0000 & ver) >> 8) | ((0xff000000 & ver) >> 24);

    char buf[16]{};
    std::snprintf(buf, sizeof(buf), "%08X", ver);
    version = buf;
    LG2_DEBUG("MAX10 {CHIP} version {VER}", "CHIP", chip_, "VER", version);
    co_return true;
}

} // namespace phosphor::software::cpld
