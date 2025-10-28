#include "tda38640a.hpp"

#include "common/include/i2c/i2c.hpp"
#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

enum class TDA38640ACmd : uint8_t
{
    CRC_LOW_REG = 0xB0,
    CRC_HIGH_REG = 0xAE,
    USER_WR_REMAIN = 0xB8,
    UNLOCK_REGS_REG = 0xD4,
    PROG_CMD_LOW_REG = 0xD6,
    PROG_CMD_HIGH_REG = 0xD7,
    REVISION_REG = 0xFD, // The silicon version value is stored in register
                         // 0x00FD [7:0] of page 0.
    PAGE_REG = 0xff
};

TDA38640A::TDA38640A(sdbusplus::async::context& ctx, uint16_t bus,
                     uint16_t address) :
    VoltageRegulator(ctx), i2cInterface(phosphor::i2c::I2C(bus, address))
{}

sdbusplus::async::task<bool> TDA38640A::getUserRemainingWrites(uint8_t* remain)
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;
    uint16_t remainBits;

    if (!(co_await setPage(0)))
    {
        error("getUserRemainingWrites failed at setPage");
        co_return false;
    }

    tbuf = buildByteVector(TDA38640ACmd::USER_WR_REMAIN);
    rbuf.resize(2);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("getUserRemainingWrites failed with sendreceive");
        co_return false;
    }
    remainBits = rbuf[0] | (rbuf[1] << 8);

    *remain = (16 - std::popcount(remainBits));

    co_return true;
}

sdbusplus::async::task<bool> TDA38640A::setPage(uint8_t page)
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(TDA38640ACmd::PAGE_REG, page);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("setPage failed with sendreceive");
        co_return false;
    }
    co_return true;
}

sdbusplus::async::task<bool> TDA38640A::getDeviceRevision(uint8_t* revision)
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    if (!(co_await setPage(0)))
    {
        error("getDeviceRevision failed at setPage");
        co_return false;
    }

    tbuf = buildByteVector(TDA38640ACmd::REVISION_REG);
    rbuf.resize(1);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("getDeviceRevision failed with sendreceive");
        co_return false;
    }

    *revision = rbuf[0];

    co_return true;
}

sdbusplus::async::task<bool> TDA38640A::getCRC(uint32_t* sum)
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    uint32_t checksum;

    if (!(co_await setPage(0)))
    {
        error("getCRC failed at setPage");
        co_return false;
    }

    tbuf = buildByteVector(TDA38640ACmd::CRC_LOW_REG);
    rbuf.resize(2);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("getCRC failed with sendreceive");
        co_return false;
    }

    checksum = rbuf[0] | (rbuf[1] << 8);

    tbuf = buildByteVector(TDA38640ACmd::CRC_HIGH_REG);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("getCRC failed with sendreceive");
        co_return false;
    }

    checksum |= (rbuf[0] << 16) | (rbuf[1] << 24);

    *sum = checksum;

    co_return true;
}

bool TDA38640A::parseImage(const uint8_t* image, size_t imageSize)
{
    std::string content(reinterpret_cast<const char*>(image), imageSize);
    std::istringstream imageStream(content);
    std::string line;

    configuration.clear();

    bool inConfigData = false;
    while (std::getline(imageStream, line))
    {
        // std::cout << line << std::endl;

        if (line.find("Part Number :") != std::string::npos)
        {
            if (line.back() == '\r')
            {
                line.pop_back();
            }
            std::string s(1, line.back());
            configuration.rev =
                static_cast<uint8_t>(std::stoul(s, nullptr, 16));
        }

        if (line.find("Configuration Checksum :") != std::string::npos)
        {
            size_t pos = line.find("0x");
            if (pos != std::string::npos)
            {
                std::string hexStr = line.substr(pos + 2);
                configuration.checksum = std::stoul(hexStr, nullptr, 16);
            }
        }

        if (line.find("[Configuration Data]") != std::string::npos)
        {
            inConfigData = true;
            continue;
        }
        if (line.find("[End Configuration Data]") != std::string::npos)
        {
            break;
        }
        if (inConfigData && !line.empty())
        {
            std::istringstream lineStream(line);
            std::string seg;
            std::vector<uint8_t> dataVector;
            while (lineStream >> seg)
            {
                if (seg.length() == 2)
                {
                    uint8_t data =
                        static_cast<uint8_t>(std::stoi(seg, nullptr, 16));
                    dataVector.push_back(data);
                }
                else
                {
                    uint16_t offset =
                        static_cast<uint16_t>(std::stoi(seg, nullptr, 16));
                    configuration.offsets.push_back(offset);
                }
            }
            configuration.data.push_back(dataVector);
        }
    }

    if (configuration.offsets.size() != configuration.data.size())
    {
        error("parseImage failed. Data line mismatch.");
        return false;
    }

    return true;
}

sdbusplus::async::task<bool> TDA38640A::unlockDevice()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(TDA38640ACmd::UNLOCK_REGS_REG,
                           static_cast<uint8_t>(0x03));
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("unlockDevice failed with sendreceive");
        co_return false;
    }
    co_return true;
}

sdbusplus::async::task<bool> TDA38640A::programmingCmd()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    if (!(co_await setPage(0)))
    {
        error("programmingCmd failed at setPage 0.");
        co_return false;
    }

    tbuf = buildByteVector(TDA38640ACmd::PROG_CMD_HIGH_REG,
                           static_cast<uint8_t>(0x3f));
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("programmingCmd high bit failed with sendreceive.");
        co_return false;
    }

    tbuf = buildByteVector(TDA38640ACmd::PROG_CMD_LOW_REG,
                           static_cast<uint8_t>(0x42));
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("programmingCmd low bit failed with sendreceive.");
        co_return false;
    }
    co_return true;
}

sdbusplus::async::task<bool> TDA38640A::getProgStatus(uint8_t* status)
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(TDA38640ACmd::PROG_CMD_HIGH_REG);
    rbuf.resize(1);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("getProgStatus failed with sendreceive");
        co_return false;
    }

    *status = rbuf[0];

    co_return true;
}

sdbusplus::async::task<bool> TDA38640A::program()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;
    uint8_t status;
    uint8_t retry = 3;

    if (!(co_await unlockDevice()))
    {
        error("program failed at unlockDevice");
        co_return false;
    }

    uint8_t page = 0;
    uint8_t address = 0;
    for (size_t i = 0; i < configuration.offsets.size(); i++)
    {
        page = configuration.offsets[i] >> 8;
        if (!(co_await setPage(page)))
        {
            error("program failed at setPage");
            co_return false;
        }

        for (uint8_t bias = 0; bias < 16; bias++)
        {
            uint16_t full_addr = configuration.offsets[i] + bias;

            if (user_section_otp_register.find(full_addr) ==
                user_section_otp_register.end())
            {
                debug(
                    "program at address {ADDR} not belone to user_section_otp_register.",
                    "ADDR", lg2::hex, full_addr);
                continue;
            }

            address = (configuration.offsets[i] & 0xFF) + bias;

            tbuf = buildByteVector(address, configuration.data[i][bias]);
            if (!i2cInterface.sendReceive(tbuf, rbuf))
            {
                error("program failed with sendreceive");
                co_return false;
            }
            debug("programming : at {PAGE} {ADDR} with {DATA}", "PAGE",
                  lg2::hex, page, "ADDR", lg2::hex, address, "DATA", lg2::hex,
                  configuration.data[i][bias]);
        }
    }

    if (!(co_await programmingCmd()))
    {
        error("program failed at programmingCmd");
        co_return false;
    }

    for (uint8_t r = 0; r < retry; r++)
    {
        co_await sdbusplus::async::sleep_for(ctx,
                                             std::chrono::milliseconds(300));

        if (!(co_await getProgStatus(&status)))
        {
            error("program failed at getProgStatus");
            co_return false;
        }

        if ((status & 0x80) == 0 || (status & 0x40) != 0)
        {
            if ((status & 0x80) == 0)
            {
                error("getProgStatus failed with 0x00D7[7] == 0, retry...");
            }
            if ((status & 0x40) != 0)
            {
                error("getProgStatus failed with 0x00D7[6] == 1, retry...");
            }
        }
        else
        {
            debug("ProgStatus ok.");
            co_return true;
        }
    }
    co_return false;
}

sdbusplus::async::task<bool> TDA38640A::verifyImage(const uint8_t* image,
                                                    size_t imageSize)
{
    uint8_t remain = 0;
    uint8_t devRev = 0;
    uint32_t devCrc = 0;

    if (!parseImage(image, imageSize))
    {
        error("verifyImage failed at parseImage");
        co_return false;
    }

    if (!(co_await getUserRemainingWrites(&remain)))
    {
        error("program failed at getUserRemainingWrites");
        co_return false;
    }
    debug("User Remaining Writes from device: {REMAIN}", "REMAIN", lg2::dec,
          remain);

    if (!remain)
    {
        error("program failed with no user remaining writes left on device");
        co_return false;
    }

    if (!(co_await getDeviceRevision(&devRev)))
    {
        error("program failed at getDeviceRevision");
        co_return false;
    }
    debug("Device revision read from device: {REV}", "REV", lg2::hex, devRev);

    if (devRev != configuration.rev)
    {
        error(
            "program failed with revision of device and configuration are not equal");
        co_return false;
    }

    if (!(co_await getCRC(&devCrc)))
    {
        error("program failed at getCRC");
        co_return false;
    }

    debug("CRC from device: {CRC}", "CRC", lg2::hex, devCrc);
    debug("CRC from config: {CRC}", "CRC", lg2::hex, configuration.checksum);

    if (devCrc == configuration.checksum)
    {
        error("program failed with same CRC value at device and configuration");
        co_return false;
    }

    co_return true;
}

bool TDA38640A::forcedUpdateAllowed()
{
    return true;
}

sdbusplus::async::task<bool> TDA38640A::updateFirmware(bool force)
{
    (void)force;
    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    if (!(co_await program()))
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    {
        error("programing TDA38640A failed");
        co_return false;
    }

    co_return true;
}

} // namespace phosphor::software::VR
