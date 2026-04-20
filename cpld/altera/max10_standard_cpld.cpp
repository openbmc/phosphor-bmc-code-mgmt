#include "max10_standard_cpld.hpp"

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
#include <vector>

namespace phosphor::software::cpld
{

namespace
{
constexpr uint32_t statusBusyErase = 0x01;
constexpr uint32_t statusBusyWrite = 0x02;
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

// Hardware constants
constexpr uint32_t ctrlRegOffset = 0x04;
constexpr uint32_t sectorEraseMask = 0x7U;
constexpr uint32_t sectorEraseShift = 20;
constexpr uint32_t wordSize = 4;
constexpr uint32_t protectAllMask = 0x1F;
constexpr uint32_t protectAllShift = 5;
constexpr uint32_t unprotectAllMask = 0xFFFFFFFF;

// Delay and retry constants
constexpr auto delayRetry = std::chrono::milliseconds(20);
constexpr auto delayWrite = std::chrono::microseconds(100);
constexpr auto delayErase = std::chrono::milliseconds(100);
constexpr auto delayBusy = std::chrono::microseconds(10);
constexpr int maxRetry = 3;

} // namespace

Max10StandardCPLD::Max10StandardCPLD(
    sdbusplus::async::context& ctx, const uint16_t bus, const uint8_t address,
    const std::string& chip, const std::string& configType,
    const Max10Profile& profile) :
    ctx(ctx), bus(bus), address(address), chip(chip), configType(configType),
    profile(profile)
{
    openDevice();
}

Max10StandardCPLD::~Max10StandardCPLD()
{
    closeDevice();
}

bool Max10StandardCPLD::openDevice()
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
        lg2::error("Failed to set I2C address {ADDR}: {ERR}", "ADDR", lg2::hex,
                   address, "ERR", std::strerror(errno));
        closeDevice();
        return false;
    }
    return true;
}

void Max10StandardCPLD::closeDevice()
{
    if (fd >= 0)
    {
        close(fd);
        fd = -1;
    }
}

sdbusplus::async::task<bool> Max10StandardCPLD::readReg(uint32_t reg,
                                                        uint32_t& value)
{
    if (fd < 0)
    {
        co_return false;
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

    int retry = maxRetry;
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
            co_return true;
        }
        co_await sdbusplus::async::sleep_for(ctx, delayRetry);
    }

    lg2::error("I2C read reg {REG} failed via ioctl: {ERR}", "REG", lg2::hex,
               reg, "ERR", std::strerror(errno));
    co_return false;
}

sdbusplus::async::task<bool> Max10StandardCPLD::writeReg(uint32_t reg,
                                                         uint32_t value)
{
    if (fd < 0)
    {
        co_return false;
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

    int retry = maxRetry;
    while (retry--)
    {
        if (ioctl(fd, I2C_RDWR, &msgSet) >= 0)
        {
            // Give bridge time to process write using async sleep
            co_await sdbusplus::async::sleep_for(ctx, delayWrite);
            co_return true;
        }
        co_await sdbusplus::async::sleep_for(ctx, delayRetry);
    }

    lg2::error("I2C write reg {REG} failed via ioctl: {ERR}", "REG", lg2::hex,
               reg, "ERR", std::strerror(errno));
    co_return false;
}

sdbusplus::async::task<bool> Max10StandardCPLD::readStatus(uint32_t& status)
{
    co_return co_await readReg(profile.csrBase + 0x00, status);
}

bool Max10StandardCPLD::validateProfile() const
{
    if (profile.startAddr >= profile.endAddr)
    {
        lg2::error("Invalid CFM range for {CHIP}", "CHIP", chip);
        return false;
    }
    if (profile.imageType == Max10ImageType::unknown)
    {
        lg2::error("Invalid image type for {CHIP}", "CHIP", chip);
        return false;
    }
    return true;
}

sdbusplus::async::task<bool> Max10StandardCPLD::protectSectors()
{
    uint32_t ctrl = 0;
    if (!(co_await readReg(profile.csrBase + ctrlRegOffset, ctrl)))
    {
        co_return false;
    }
    ctrl |= (protectAllMask << protectAllShift);
    co_return co_await writeReg(profile.csrBase + ctrlRegOffset, ctrl);
}

sdbusplus::async::task<bool> Max10StandardCPLD::unprotectSector(int sectorId)
{
    uint32_t ctrl = 0;
    if (!(co_await readReg(profile.csrBase + ctrlRegOffset, ctrl)))
    {
        co_return false;
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
            ctrl = unprotectAllMask;
            break;
        default:
            lg2::error("Unknown sector id {ID}", "ID", sectorId);
            co_return false;
    }

    co_return co_await writeReg(profile.csrBase + ctrlRegOffset, ctrl);
}

sdbusplus::async::task<bool> Max10StandardCPLD::eraseSector(int sectorId)
{
    uint32_t ctrl = 0;
    if (!(co_await readReg(profile.csrBase + ctrlRegOffset, ctrl)))
    {
        co_return false;
    }

    ctrl &= ~(sectorEraseMask << sectorEraseShift);

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
            co_return false;
    }

    if (!(co_await writeReg(profile.csrBase + ctrlRegOffset, ctrl)))
    {
        co_return false;
    }

    co_return co_await waitEraseDone();
}

sdbusplus::async::task<bool> Max10StandardCPLD::waitEraseDone(int timeoutCount)
{
    co_await sdbusplus::async::sleep_for(ctx, delayErase);

    for (int cnt = 0; cnt < timeoutCount; ++cnt)
    {
        uint32_t status = 0;
        if (!(co_await readStatus(status)))
        {
            continue;
        }

        status &= statusMask;

        if (status & statusBusyErase)
        {
            co_await sdbusplus::async::sleep_for(ctx, delayBusy);
            continue;
        }

        if (status & statusEraseSuccess)
        {
            co_return true;
        }

        if (status != 0)
        {
            lg2::error("Erase failed, status={STATUS}", "STATUS", lg2::hex,
                       status);
            co_return false;
        }
    }
    lg2::error("Erase timeout");
    co_return false;
}

sdbusplus::async::task<bool> Max10StandardCPLD::waitWriteDone(int timeoutCount)
{
    for (int cnt = 0; cnt < timeoutCount; ++cnt)
    {
        uint32_t status = 0;
        if (!(co_await readStatus(status)))
        {
            continue;
        }

        status &= statusMask;

        if (status & statusBusyWrite)
        {
            co_await sdbusplus::async::sleep_for(ctx, delayBusy);
            continue;
        }

        if (status & statusWriteSuccess)
        {
            co_return true;
        }

        if (status != 0)
        {
            lg2::error("Write failed, status={STATUS}", "STATUS", lg2::hex,
                       status);
            co_return false;
        }
    }
    lg2::error("Write timeout");
    co_return false;
}

uint8_t Max10StandardCPLD::bitReverse(uint8_t value)
{
    /*Swap LSB with MSB before write into CFM*/
    value = static_cast<uint8_t>((value & 0xF0) >> 4 | (value & 0x0F) << 4);
    value = static_cast<uint8_t>((value & 0xCC) >> 2 | (value & 0x33) << 2);
    value = static_cast<uint8_t>((value & 0xAA) >> 1 | (value & 0x55) << 1);
    return value;
}

uint32_t Max10StandardCPLD::packWord(const uint8_t* data)
{
    return (static_cast<uint32_t>(bitReverse(data[0])) << 24) |
           (static_cast<uint32_t>(bitReverse(data[1])) << 16) |
           (static_cast<uint32_t>(bitReverse(data[2])) << 8) |
           (static_cast<uint32_t>(bitReverse(data[3])) << 0);
}

sdbusplus::async::task<bool> Max10StandardCPLD::programRpd(
    const uint8_t* image, size_t imageSize,
    const std::function<bool(int)>& progressCallback)
{
    constexpr int baseProgressPercent = 10;
    constexpr int programProgressPercent = 85;
    constexpr int completeProgressPercent = 100;
    constexpr size_t updateProgressInterval = 1024;

    if (!validateProfile())
    {
        co_return false;
    }

    const auto expectedSize =
        static_cast<size_t>(profile.endAddr - profile.startAddr);
    if (imageSize != expectedSize)
    {
        lg2::error("Unsupported image type {TYPE}", "TYPE",
                   static_cast<int>(profile.imageType));
        co_return false;
    }

    switch (profile.imageType)
    {
        case Max10ImageType::cfmImage1:
            if (!(co_await unprotectSector(5)) || !(co_await eraseSector(5)))
            {
                lg2::error("Erase or unprotect failed for sector 5");
                co_return false;
            }
            break;
        case Max10ImageType::cfmImage2:
            if (!(co_await unprotectSector(3)) ||
                !(co_await unprotectSector(4)) || !(co_await eraseSector(3)) ||
                !(co_await eraseSector(4)))
            {
                lg2::error("Erase or unprotect failed for sector 3 or 4");
                co_return false;
            }
            break;
        case Max10ImageType::cfmImage3:
            if (!(co_await unprotectSector(4)) || !(co_await eraseSector(4)))
            {
                lg2::error("Erase or unprotect failed for sector 4");
                co_return false;
            }
            break;
        default:
            lg2::error("Unsupported image type {TYPE}", "TYPE",
                       static_cast<int>(profile.imageType));
            co_return false;
    }

    // Set erase none
    co_await eraseSector(0);

    if (progressCallback)
    {
        progressCallback(baseProgressPercent);
    }

    // start program
    size_t offset = 0;
    for (uint32_t addr = profile.startAddr; addr < profile.endAddr;
         addr += wordSize, offset += wordSize)
    {
        // Check if there are enough bytes left for a full word
        if ((offset + (wordSize - 1)) >= imageSize)
        {
            break;
        }

        const uint32_t word = packWord(image + offset);

        /*Command to write into On-Chip Flash IP*/
        if (!(co_await writeReg(profile.dataBase + addr, word)))
        {
            co_return false;
        }

        if (!(co_await waitWriteDone()))
        {
            co_return false;
        }

        if (progressCallback && (offset % updateProgressInterval == 0))
        {
            const int progressPercent =
                baseProgressPercent +
                static_cast<int>((offset * programProgressPercent) / imageSize);
            progressCallback(progressPercent);
        }
    }

    if (!(co_await protectSectors()))
    {
        co_return false;
    }

    if (progressCallback)
    {
        progressCallback(completeProgressPercent);
    }

    co_return true;
}

sdbusplus::async::task<bool> Max10StandardCPLD::updateFirmware(
    bool force, const uint8_t* image, size_t imageSize,
    std::function<bool(int)> progressCallback)
{
    (void)force;

    if (image == nullptr || imageSize == 0)
    {
        lg2::error("MAX10 image is empty");
        co_return false;
    }

    if (fd < 0)
    {
        co_return false;
    }

    bool result = co_await programRpd(image, imageSize, progressCallback);

    co_return result;
}

sdbusplus::async::task<bool> Max10StandardCPLD::getVersion(std::string& version)
{
    constexpr uint32_t versionReg = 0x00100028;
    constexpr size_t versionBufSize = 16;

    if (fd < 0)
    {
        co_return false;
    }

    uint32_t ver = 0;
    if (!(co_await readReg(versionReg, ver)))
    {
        co_return false;
    }

    ver = ((0x000000ff & ver) << 24) | ((0x0000ff00 & ver) << 8) |
          ((0x00ff0000 & ver) >> 8) | ((0xff000000 & ver) >> 24);

    char buf[versionBufSize]{};
    std::snprintf(buf, sizeof(buf), "%08X", ver);
    version = buf;

    co_return true;
}

} // namespace phosphor::software::cpld
