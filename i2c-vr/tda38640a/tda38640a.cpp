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

static constexpr size_t progNVMDelay = 300;
static constexpr uint8_t NVMDoneMask = 0x80;
static constexpr uint8_t NVMErrorMask = 0x40;
static constexpr uint8_t pageZero = 0;

enum class TDA38640ACmd : uint8_t
{
    crcLowReg = 0xB0,
    crcHighReg = 0xAE,
    userWrRemain = 0xB8,
    unlockRegsReg = 0xD4,
    unlockRegsVal = 0x03, // Unlock i2c and PMBus address registers.
    progCmdLowReg = 0xD6,
    progCmdHighReg = 0xD7,
    progCmdLowVal = 0x42, // 0x3f42 From datasheet, This will store the user
                          // register in the next available nvm user image.
    progCmdHighVal = 0x3F,
    revisionReg = 0xFD,   // The silicon version value is stored in register
                          // 0x00FD [7:0] of page 0.
    pageReg = 0xff
};

const std::unordered_set<uint16_t> user_section_otp_register{
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048,
    0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F, 0x0050, 0x0051,
    0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A,
    0x005B, 0x005C, 0x005D, 0x005E, 0x005F, 0x0060, 0x0061, 0x0062, 0x0063,
    0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C,
    0x006D, 0x006E, 0x006F, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075,
    0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B, 0x0202, 0x0204, 0x0220,
    0x0240, 0x0242, 0x0243, 0x0248, 0x0249, 0x024A, 0x024B, 0x024C, 0x024D,
    0x024E, 0x024F, 0x0250, 0x0251, 0x0252, 0x0256, 0x0257, 0x0266, 0x0267,
    0x026A, 0x026C, 0x0270, 0x0272, 0x0273, 0x0280, 0x0281, 0x0282, 0x0288,
    0x0289, 0x028A, 0x028C, 0x028D, 0x028E, 0x029E, 0x02A0, 0x02A2, 0x02AA,
    0x02AB, 0x02AC, 0x02BC, 0x02BD, 0x02BE, 0x02BF, 0x02C0, 0x02C2, 0x02C8,
    0x02CA, 0x0384, 0x0385};

TDA38640A::TDA38640A(sdbusplus::async::context& ctx, uint16_t bus,
                     uint16_t address) :
    VoltageRegulator(ctx), i2cInterface(phosphor::i2c::I2C(bus, address))
{}

sdbusplus::async::task<bool> TDA38640A::getUserRemainingWrites(uint8_t* remain)
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;
    uint16_t remainBits = 0;

    if (!(co_await setPage(pageZero)))
    {
        error("getUserRemainingWrites failed at setPage");
        co_return false;
    }

    tbuf = buildByteVector(TDA38640ACmd::userWrRemain);
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

    tbuf = buildByteVector(TDA38640ACmd::pageReg, page);
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

    if (!(co_await setPage(pageZero)))
    {
        error("getDeviceRevision failed at setPage");
        co_return false;
    }

    tbuf = buildByteVector(TDA38640ACmd::revisionReg);
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

    uint32_t checksum = 0;

    if (!(co_await setPage(pageZero)))
    {
        error("getCRC failed at setPage");
        co_return false;
    }

    tbuf = buildByteVector(TDA38640ACmd::crcLowReg);
    rbuf.resize(2);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("getCRC failed with sendreceive");
        co_return false;
    }

    checksum = rbuf[0] | (rbuf[1] << 8);

    tbuf = buildByteVector(TDA38640ACmd::crcHighReg);
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

    tbuf = buildByteVector(TDA38640ACmd::unlockRegsReg,
                           TDA38640ACmd::unlockRegsVal);
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

    if (!(co_await setPage(pageZero)))
    {
        error("programmingCmd failed at setPage 0.");
        co_return false;
    }

    tbuf = buildByteVector(TDA38640ACmd::progCmdHighReg,
                           TDA38640ACmd::progCmdHighVal);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("programmingCmd high bit failed with sendreceive.");
        co_return false;
    }

    tbuf = buildByteVector(TDA38640ACmd::progCmdLowReg,
                           TDA38640ACmd::progCmdLowVal);
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

    tbuf = buildByteVector(TDA38640ACmd::progCmdHighReg);
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
        co_await sdbusplus::async::sleep_for(
            ctx, std::chrono::milliseconds(progNVMDelay));

        if (!(co_await getProgStatus(&status)))
        {
            error("program failed at getProgStatus");
            co_return false;
        }

        if ((status & NVMDoneMask) == 0 || (status & NVMErrorMask) != 0)
        {
            if ((status & NVMDoneMask) == 0)
            {
                error(
                    "getProgStatus failed with 0x00D7[7] == 0, Programming command not completed. retry...");
            }
            if ((status & NVMErrorMask) != 0)
            {
                error(
                    "getProgStatus failed with 0x00D7[6] == 1, The previous NVM operation encountered an error. retry...");
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
    if (!(co_await program()))
    {
        error("programing TDA38640A failed");
        co_return false;
    }

    co_return true;
}

} // namespace phosphor::software::VR
