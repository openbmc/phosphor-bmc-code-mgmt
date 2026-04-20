#include "max10_cpld.hpp"

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

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
#include <vector>

namespace phosphor::software::cpld
{

namespace
{
constexpr uint32_t versionReg = 0x00100028;

constexpr uint32_t statusBusyErase = 0x01;
constexpr uint32_t statusBusyWrite = 0x02;
constexpr uint32_t statusBusyRead = 0x03;
constexpr uint32_t statusReadSuccess = 0x04;
constexpr uint32_t statusWriteSuccess = 0x08;
constexpr uint32_t statusEraseSuccess = 0x10;
constexpr uint32_t statusMask = 0x1F;

constexpr uint32_t protectSec5 = 0x1U << 27;
constexpr uint32_t protectSec4 = 0x1U << 26;
constexpr uint32_t protectSec3 = 0x1U << 25;
constexpr uint32_t protectSec2 = 0x1U << 24;
constexpr uint32_t protectSec1 = 0x1U << 23;

constexpr uint32_t sectorErase5 = 0b101U << 20;
constexpr uint32_t sectorErase4 = 0b100U << 20;
constexpr uint32_t sectorErase3 = 0b011U << 20;
constexpr uint32_t sectorErase2 = 0b010U << 20;
constexpr uint32_t sectorErase1 = 0b001U << 20;
constexpr uint32_t sectorEraseNone = 0b111U << 20;
} // namespace

Max10CPLD::Max10CPLD(sdbusplus::async::context& /*ctx*/, const uint16_t bus,
                     const uint8_t address, const std::string& chip,
                     const std::string& configType, const bool debugMode,
                     const Max10Profile& profile) :
    bus(bus), address(address), chip(chip), configType(configType),
    debugMode(debugMode), profile(profile)
{}

Max10CPLD::~Max10CPLD()
{
    closeDevice();
}

bool Max10CPLD::ensureOpen()
{
    if (fd >= 0)
    {
        return true;
    }

    std::string path = "/dev/i2c-" + std::to_string(bus);
    fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        lg2::error("Failed to open {PATH}: {ERR}", "PATH", path, "ERR",
                   std::strerror(errno));
        return false;
    }

    if (ioctl(fd, I2C_SLAVE, address) < 0)
    {
        lg2::error("Failed to set I2C address 0x{ADDR:02X}: {ERR}", "ADDR",
                   address, "ERR", std::strerror(errno));
        closeDevice();
        return false;
    }
    return true;
}

void Max10CPLD::closeDevice()
{
    if (fd >= 0)
    {
        close(fd);
        fd = -1;
    }
}

bool Max10CPLD::readReg(uint32_t reg, uint32_t& value)
{
    if (!ensureOpen())
    {
        return false;
    }

    std::array<uint8_t, 4> addrBuf;
    addrBuf[0] = (reg >> 24) & 0xFF;
    addrBuf[1] = (reg >> 16) & 0xFF;
    addrBuf[2] = (reg >> 8) & 0xFF;
    addrBuf[3] = reg & 0xFF;

    std::array<uint8_t, 4> dataBuf{};
    struct i2c_msg msgs[2] = {};

    msgs[0].addr = address;
    msgs[0].flags = 0;
    msgs[0].len = addrBuf.size();
    msgs[0].buf = addrBuf.data();

    msgs[1].addr = address;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = dataBuf.size();
    msgs[1].buf = dataBuf.data();

    struct i2c_rdwr_ioctl_data msgSet = {};
    msgSet.msgs = msgs;
    msgSet.nmsgs = 2;

    int retry = 3;
    while (retry--)
    {
        if (ioctl(fd, I2C_RDWR, &msgSet) >= 0)
        {
            if (profile.littleEndian)
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
    {
        return false;
    }

    std::array<uint8_t, 8> data{};
    data[0] = (reg >> 24) & 0xFF;
    data[1] = (reg >> 16) & 0xFF;
    data[2] = (reg >> 8) & 0xFF;
    data[3] = reg & 0xFF;

    if (profile.littleEndian)
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
    msg.addr = address;
    msg.flags = 0;
    msg.len = data.size();
    msg.buf = data.data();

    struct i2c_rdwr_ioctl_data msgSet = {};
    msgSet.msgs = &msg;
    msgSet.nmsgs = 1;

    int retry = 3;
    while (retry--)
    {
        if (ioctl(fd, I2C_RDWR, &msgSet) >= 0)
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
    return readReg(profile.csrBase + 0x00, status);
}

bool Max10CPLD::validateProfile() const
{
    if (profile.startAddr >= profile.endAddr)
    {
        lg2::error("Invalid CFM range for {CHIP}", "CHIP", chip);
        return false;
    }
    if (profile.imageType == 0)
    {
        lg2::error("Invalid image type for {CHIP}", "CHIP", chip);
        return false;
    }
    return true;
}

bool Max10CPLD::protectSectors()
{
    uint32_t ctrl = 0;
    if (!readReg(profile.csrBase + 0x04, ctrl))
    {
        return false;
    }
    ctrl |= (0x1F << 5);
    return writeReg(profile.csrBase + 0x04, ctrl);
}

bool Max10CPLD::unprotectSector(int sectorId)
{
    uint32_t ctrl = 0;
    if (!readReg(profile.csrBase + 0x04, ctrl))
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

    return writeReg(profile.csrBase + 0x04, ctrl);
}

bool Max10CPLD::eraseSector(int sectorId)
{
    uint32_t ctrl = 0;
    if (!readReg(profile.csrBase + 0x04, ctrl))
    {
        return false;
    }

    ctrl &= ~(0x7U << 20);

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

    if (!writeReg(profile.csrBase + 0x04, ctrl))
    {
        return false;
    }

    return waitEraseDone();
}

bool Max10CPLD::waitEraseDone(int timeoutCount)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for (int cnt = 0; cnt < timeoutCount; ++cnt)
    {
        uint32_t status = 0;
        if (!readStatus(status))
        {
            continue;
        }

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
        {
            continue;
        }

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

uint8_t Max10CPLD::bitReverse(uint8_t value)
{
    /*Swap LSB with MSB before write into CFM*/
    value = static_cast<uint8_t>((value & 0xF0) >> 4 | (value & 0x0F) << 4);
    value = static_cast<uint8_t>((value & 0xCC) >> 2 | (value & 0x33) << 2);
    value = static_cast<uint8_t>((value & 0xAA) >> 1 | (value & 0x55) << 1);
    return value;
}

uint32_t Max10CPLD::packWord(const uint8_t* data)
{
    return (static_cast<uint32_t>(bitReverse(data[0])) << 24) |
           (static_cast<uint32_t>(bitReverse(data[1])) << 16) |
           (static_cast<uint32_t>(bitReverse(data[2])) << 8) |
           (static_cast<uint32_t>(bitReverse(data[3])) << 0);
}

std::string Max10CPLD::toHex8(uint32_t value)
{
    char buf[11]{};
    std::snprintf(buf, sizeof(buf), "0x%08X", value);
    return buf;
}

bool Max10CPLD::programRpd(const uint8_t* image, size_t imageSize,
                           const std::function<bool(int)>& progressCallback)
{
    if (!validateProfile())
    {
        return false;
    }

    const auto expectedSize =
        static_cast<size_t>(profile.endAddr - profile.startAddr);
    if (imageSize != expectedSize)
    {
        lg2::error("Unsupported image type {TYPE}", "TYPE", profile.imageType);
        return false;
    }

    int ret = 0;
    switch (profile.imageType)
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
                       profile.imageType);
            return false;
    }

    if (ret != 0)
    {
        lg2::error("Erase or unprotect failed");
        return false;
    }

    // Set erase none
    eraseSector(0);

    if (progressCallback)
    {
        progressCallback(10);
    }

    // start program
    size_t offset = 0;
    for (uint32_t addr = profile.startAddr; addr < profile.endAddr;
         addr += 4, offset += 4)
    {
        if ((offset + 3) >= imageSize)
        {
            break;
        }

        const uint32_t word = packWord(image + offset);

        /*Command to write into On-Chip Flash IP*/
        if (!writeReg(profile.dataBase + addr, word))
        {
            return false;
        }

        if (!waitWriteDone())
        {
            return false;
        }

        if (progressCallback && (offset % 1024 == 0))
        {
            const int pct = 10 + static_cast<int>((offset * 85) / imageSize);
            progressCallback(pct);
        }
    }

    if (!protectSectors())
    {
        return false;
    }

    if (progressCallback)
    {
        progressCallback(100);
    }

    return true;
}

sdbusplus::async::task<bool> Max10CPLD::updateFirmware(
    bool force, const uint8_t* image, size_t imageSize,
    std::function<bool(int)> progressCallback)
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

    bool result = programRpd(image, imageSize, progressCallback);

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

    co_return true;
}

} // namespace phosphor::software::cpld
