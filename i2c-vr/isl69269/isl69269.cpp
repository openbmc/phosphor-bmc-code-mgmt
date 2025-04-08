#include "isl69269.hpp"

#include "common/include/i2c/i2c.hpp"

#include <phosphor-logging/lg2.hpp>

#include <string>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

constexpr uint8_t regProgStatus = 0x7E;
constexpr uint8_t regHexModeCFG0 = 0x87;
constexpr uint8_t regCRC = 0x94;
constexpr uint8_t regHexModeCFG1 = 0xBD;
constexpr uint8_t regDMAData = 0xC5;
constexpr uint8_t regDMAAddr = 0xC7;
constexpr uint8_t regRestoreCfg = 0xF2;

constexpr uint8_t regRemainginWrites = 0x35;

constexpr uint8_t gen3SWRevMin = 0x06;
constexpr uint8_t deviceIdLength = 4;

constexpr uint8_t gen3Legacy = 1;
constexpr uint8_t gen3Production = 2;

constexpr uint16_t cfgId = 7;
constexpr uint16_t gen3FileHead = 5;
constexpr uint16_t gen3LegacyCRC = 276 - gen3FileHead;
constexpr uint16_t gen3ProductionCRC = 290 - gen3FileHead;
constexpr uint8_t checksumLen = 4;
constexpr uint8_t deviceRevisionLen = 4;

// Common pmBus Command codes
constexpr uint8_t pmBusDeviceId = 0xAD;
constexpr uint8_t pmBusDeviceRev = 0xAE;

// Config file constants
constexpr char recordTypeData = 0x00;
constexpr char recordTypeHeader = 0x49;

constexpr uint8_t defaultBufferSize = 16;
constexpr uint8_t programBufferSize = 32;

constexpr uint8_t zeroByteLen = 0;
constexpr uint8_t oneByteLen = 1;
constexpr uint8_t threeByteLen = 3;
constexpr uint8_t fourByteLen = 4;

ISL69269::ISL69269(sdbusplus::async::context& ctx, uint16_t bus,
                   uint16_t address) :
    VoltageRegulator(ctx), i2cInterface(phosphor::i2c::I2C(bus, address))
{}

inline void shiftLeftFromLSB(const uint8_t* data, uint32_t* result)
{
    *result = (static_cast<uint32_t>(data[3]) << 24) |
              (static_cast<uint32_t>(data[2]) << 16) |
              (static_cast<uint32_t>(data[1]) << 8) |
              (static_cast<uint32_t>(data[0]));
}

inline void shiftLeftFromMSB(const uint8_t* data, uint32_t* result)
{
    *result = (static_cast<uint32_t>(data[0]) << 24) |
              (static_cast<uint32_t>(data[1]) << 16) |
              (static_cast<uint32_t>(data[2]) << 8) |
              (static_cast<uint32_t>(data[3]));
}

static uint8_t calcCRC8(const uint8_t* data, uint8_t len)
{
    uint8_t crc = 0x00;
    int i = 0;
    int b = 0;

    for (i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (b = 0; b < 8; b++)
        {
            if (crc & 0x80)
            {
                crc = (crc << 1) ^ 0x07; // polynomial 0x07
            }
            else
            {
                crc = (crc << 1);
            }
        }
    }

    return crc;
}

sdbusplus::async::task<bool> ISL69269::dmaReadWrite(uint8_t* reg, uint8_t* resp)
{
    uint8_t tbuf[defaultBufferSize] = {0};
    uint8_t tlen = threeByteLen;
    uint8_t rbuf[defaultBufferSize] = {0};
    uint8_t rlen = zeroByteLen;

    tbuf[0] = regDMAData;
    std::memcpy(&tbuf[1], reg, 2);

    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    if (!(co_await i2cInterface.sendReceive(tbuf, tlen, rbuf, rlen)))
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    {
        error("dmaReadWrite failed with {CMD}", "CMD",
              std::string("_REG_DMA_ADDR"));
        co_return false;
    }

    tlen = oneByteLen;
    rlen = fourByteLen;

    tbuf[0] = regDMAAddr;
    if (!(co_await i2cInterface.sendReceive(tbuf, tlen, resp, rlen)))
    {
        error("dmaReadWrite failed with {CMD}", "CMD",
              std::string("_REG_DMA_DATA"));
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> ISL69269::getRemainingWrites(uint8_t* remain)
{
    uint8_t tbuf[defaultBufferSize] = {0};
    uint8_t rbuf[defaultBufferSize] = {0};

    tbuf[0] = regRemainginWrites;
    tbuf[1] = 0x00;
    if (!(co_await dmaReadWrite(tbuf, rbuf)))
    {
        error("getRemainingWrites failed");
        co_return false;
    }

    *remain = rbuf[0];
    co_return true;
}

sdbusplus::async::task<bool> ISL69269::getHexMode(uint8_t* mode)
{
    uint8_t tbuf[defaultBufferSize] = {0};
    uint8_t rbuf[defaultBufferSize] = {0};

    tbuf[0] = regHexModeCFG0;
    tbuf[1] = regHexModeCFG1;
    if (!(co_await dmaReadWrite(tbuf, rbuf)))
    {
        error("getHexMode failed");
        co_return false;
    }

    *mode = (rbuf[0] == 0) ? gen3Legacy : gen3Production;

    co_return true;
}

sdbusplus::async::task<bool> ISL69269::getDeviceId(uint32_t* deviceId)
{
    uint8_t tbuf[defaultBufferSize] = {0};
    uint8_t tLen = oneByteLen;
    uint8_t rbuf[defaultBufferSize] = {0};
    uint8_t rLen = deviceIdLength + 1;

    tbuf[0] = pmBusDeviceId;

    if (!(co_await i2cInterface.sendReceive(tbuf, tLen, rbuf, rLen)))
    {
        error("getDeviceId failed");
        co_return false;
    }

    std::memcpy(deviceId, &rbuf[1], deviceIdLength);

    co_return true;
}

sdbusplus::async::task<bool> ISL69269::getDeviceRevision(uint32_t* revision)
{
    uint8_t tbuf[defaultBufferSize] = {0};
    uint8_t tlen = oneByteLen;
    uint8_t rbuf[defaultBufferSize] = {0};
    uint8_t rlen = deviceRevisionLen + 1;

    tbuf[0] = pmBusDeviceRev;
    if (!(co_await i2cInterface.sendReceive(tbuf, tlen, rbuf, rlen)))
    {
        error("getDeviceRevision failed with sendreceive");
        co_return false;
    }

    if (mode == gen3Legacy)
    {
        std::memcpy(revision, &rbuf[1], deviceRevisionLen);
    }
    else
    {
        shiftLeftFromLSB(rbuf + 1, revision);
    }

    co_return true;
}

sdbusplus::async::task<bool> ISL69269::getCRC(uint32_t* sum)
{
    uint8_t tbuf[defaultBufferSize] = {0};
    uint8_t rbuf[defaultBufferSize] = {0};

    tbuf[0] = regCRC;
    if (!(co_await dmaReadWrite(tbuf, rbuf)))
    {
        error("getCRC failed");
        co_return false;
    }
    std::memcpy(sum, rbuf, sizeof(uint32_t));

    co_return true;
}

bool ISL69269::parseImage(const uint8_t* image, size_t imageSize)
{
    size_t nextLineStart = 0;
    int dcnt = 0;

    for (size_t i = 0; i < imageSize; i++)
    {
        if (image[i] == '\n') // We have a hex file, so we check new line.
        {
            char line[40];
            char xdigit[8] = {0};
            uint8_t sepLine[32] = {0};

            size_t lineLen = i - nextLineStart;
            std::memcpy(line, image + nextLineStart, lineLen);
            int k = 0;
            size_t j = 0;
            for (k = 0, j = 0; j < lineLen; k++, j += 2)
            {
                // Convert two chars into a array of single values
                std::memcpy(xdigit, &line[j], 2);
                sepLine[k] = (uint8_t)std::strtol(xdigit, NULL, 16);
            }

            if (sepLine[0] == recordTypeHeader)
            {
                if (sepLine[3] == pmBusDeviceId)
                {
                    shiftLeftFromMSB(sepLine + 4, &configuration.devIdExp);
                    debug("device id from configuration: {ID}", "ID", lg2::hex,
                          configuration.devIdExp);
                }
                else if (sepLine[3] == pmBusDeviceRev)
                {
                    shiftLeftFromMSB(sepLine + 4, &configuration.devRevExp);
                    debug("device revision from config: {ID}", "ID", lg2::hex,
                          configuration.devRevExp);

                    // According to programing guide:
                    // If legacy hex file
                    // MSB device revision == 0x00 | 0x01
                    if (configuration.devRevExp < (gen3SWRevMin << 24))
                    {
                        debug("Legacy hex file format recognized");
                        configuration.mode = gen3Legacy;
                    }
                    else
                    {
                        debug("Production hex file format recognized");
                        configuration.mode = gen3Production;
                    }
                }
            }
            else if (sepLine[0] == recordTypeData)
            {
                if (((sepLine[1] + 2) >= (uint8_t)sizeof(sepLine)))
                {
                    dcnt = 0;
                    break;
                }
                // According to documentation:
                // 00 05 C2 E7 08 00 F6
                //  |  |  |  |  |  |  |
                //  |  |  |  |  |  |  - Packet Error Code (CRC8)
                //  |  |  |  |  -  - Data
                //  |  |  |  - Command Code
                //  |  |  - Address
                //  |  - Size of data (including Addr, Cmd, CRC8)
                //  - Line type (0x00 - Data, 0x49 header information)
                configuration.pData[dcnt].len = sepLine[1] - 2;
                configuration.pData[dcnt].pec =
                    sepLine[3 + configuration.pData[dcnt].len];
                configuration.pData[dcnt].addr = sepLine[2];
                configuration.pData[dcnt].cmd = sepLine[3];
                std::memcpy(configuration.pData[dcnt].data, sepLine + 2,
                            configuration.pData[dcnt].len + 1);
                switch (dcnt)
                {
                    case cfgId:
                        configuration.cfgId = sepLine[4] & 0x0F;
                        debug("Config ID: {ID}", "ID", configuration.cfgId);
                        break;
                    case gen3LegacyCRC:
                        if (configuration.mode == gen3Legacy)
                        {
                            std::memcpy(&configuration.crcExp, &sepLine[4],
                                        checksumLen);
                            debug("Config Legacy CRC: {CRC}", "CRC", lg2::hex,
                                  configuration.crcExp);
                        }
                        break;
                    case gen3ProductionCRC:
                        if (configuration.mode == gen3Production)
                        {
                            std::memcpy(&configuration.crcExp, &sepLine[4],
                                        checksumLen);
                            debug("Config Production CRC: {CRC}", "CRC",
                                  lg2::hex, configuration.crcExp);
                        }
                        break;
                }
                dcnt++;
            }
            else
            {
                error("parseImage failed. Unknown recordType");
                return false;
            }

            nextLineStart = i + 1;
        }
    }
    configuration.wrCnt = dcnt;
    return true;
}

bool ISL69269::checkImage()
{
    uint8_t crc8 = 0;

    for (int i = 0; i < configuration.wrCnt; i++)
    {
        crc8 = calcCRC8(configuration.pData[i].data,
                        configuration.pData[i].len + 1);
        if (crc8 != configuration.pData[i].pec)
        {
            debug(
                "Config line: {LINE}, failed to calculate CRC. Have {HAVE}, Want: {WANT}",
                "LINE", i, "HAVE", lg2::hex, crc8, "WANT", lg2::hex,
                configuration.pData[i].pec);
            return false;
        }
    }

    return true;
}

sdbusplus::async::task<bool> ISL69269::program()
{
    uint8_t tbuf[programBufferSize] = {0};
    uint8_t rbuf[programBufferSize] = {0};
    uint8_t rlen = zeroByteLen;

    for (int i = 0; i < configuration.wrCnt; i++)
    {
        tbuf[0] = configuration.pData[i].cmd;
        std::memcpy(tbuf + 1, &configuration.pData[i].data,
                    configuration.pData[i].len - 1);

        if (!(co_await i2cInterface.sendReceive(
                tbuf, configuration.pData[i].len, rbuf, rlen)))
        {
            error("program failed at writing data to voltage regulator");
        }
    }

    co_return true;
}

sdbusplus::async::task<bool> ISL69269::getProgStatus()
{
    uint8_t tbuf[programBufferSize] = {0};
    uint8_t rbuf[programBufferSize] = {0};
    int retry = 3;

    tbuf[0] = regProgStatus;
    tbuf[1] = 0x00;

    do
    {
        // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
        if (!(co_await dmaReadWrite(tbuf, rbuf)))
        // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
        {
            error("getProgStatus failed on dmaReadWrite");
            co_return false;
        }

        if (rbuf[0] & 0x01)
        {
            break;
        }
        if (--retry == 0)
        {
            error("failed to program the device");
            co_return false;
        }
        co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(1));
    } while (retry > 0);

    co_return true;
}

sdbusplus::async::task<bool> ISL69269::restoreCfg()
{
    uint8_t tbuf[defaultBufferSize] = {0};
    uint8_t rbuf[defaultBufferSize] = {0};

    tbuf[0] = regRestoreCfg;
    tbuf[1] = configuration.cfgId;

    if (!(co_await dmaReadWrite(tbuf, rbuf)))
    {
        error("restoreCfg failed at dmaReadWrite");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> ISL69269::verifyImage(const uint8_t* image,
                                                   size_t imageSize)
{
    uint8_t mode = 0xFF;
    uint8_t remain = 0;
    uint32_t devID = 0;
    uint32_t devRev = 0;
    uint32_t crc = 0;

    if (!(co_await getHexMode(&mode)))
    {
        error("program failed at getHexMode");
        co_return false;
    }

    if (!parseImage(image, imageSize))
    {
        error("verifyImage failed at parseImage");
        co_return false;
    }

    if (!checkImage())
    {
        error("verifyImage failed at checkImage");
        co_return false;
    }

    if (mode != configuration.mode)
    {
        error(
            "program failed with mode of device and configuration are not equal");
        co_return false;
    }

    if (!(co_await getRemainingWrites(&remain)))
    {
        error("program failed at getRemainingWrites");
        co_return false;
    }

    if (!remain)
    {
        error("program failed with no remaining writes left on device");
        co_return false;
    }

    if (!(co_await getDeviceId(&devID)))
    {
        error("program failed at getDeviceId");
        co_return false;
    }

    if (devID != configuration.devIdExp)
    {
        error(
            "program failed with not matching device id of device and config");
        co_return false;
    }

    if (!(co_await getDeviceRevision(&devRev)))
    {
        error("program failed at getDeviceRevision");
        co_return false;
    }
    debug("Device revision read from device: {REV}", "REV", lg2::hex, devRev);

    switch (mode)
    {
        case gen3Legacy:
            if (((devRev >> 24) >= gen3SWRevMin) &&
                (configuration.devRevExp <= 0x1))
            {
                debug("Legacy mode revision checks out");
            }
            else
            {
                error(
                    "revision requirements for legacy mode device not fulfilled");
            }
            break;
        case gen3Production:
            if (((devRev >> 24) >= gen3SWRevMin) &&
                (configuration.devRevExp >= gen3SWRevMin))
            {
                debug("Production mode revision checks out");
            }
            else
            {
                error(
                    "revision requirements for production mode device not fulfilled");
            }
            break;
    }

    if (!(co_await getCRC(&crc)))
    {
        error("program failed at getCRC");
        co_return false;
    }

    if (crc == configuration.crcExp)
    {
        error("program failed with same CRC value at device and configuration");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> ISL69269::reset()
{
    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    if (!(co_await getProgStatus()))
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    {
        error("reset failed at getProgStatus");
        co_return false;
    }

    if (!(co_await restoreCfg()))
    {
        error("reset failed at restoreCfg");
        co_return false;
    }

    // Reset configuration for next update.
    configuration.addr = 0;
    configuration.mode = 0;
    configuration.cfgId = 0;
    configuration.wrCnt = 0;
    configuration.devIdExp = 0;
    configuration.devRevExp = 0;
    configuration.crcExp = 0x00;
    for (int i = 0; i < configuration.wrCnt; i++)
    {
        configuration.pData[i].pec = 0x00;
        configuration.pData[i].addr = 0x00;
        configuration.pData[i].cmd = 0x00;
        for (int j = 0; j < configuration.pData[i].len; j++)
        {
            configuration.pData[i].data[j] = 0x00;
        }
        configuration.pData[i].len = 0x00;
    }

    co_return true;
}

bool ISL69269::forcedUpdateAllowed()
{
    return true;
}

sdbusplus::async::task<bool> ISL69269::updateFirmware(bool force)
{
    (void)force;
    if (!(co_await program()))
    {
        error("programing ISL69269 failed");
        co_return false;
    }

    co_return true;
}

} // namespace phosphor::software::VR
