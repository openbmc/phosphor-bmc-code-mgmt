#include "isl69269.hpp"

#include "common/include/i2c/i2c.hpp"

#include <phosphor-logging/lg2.hpp>

#include <charconv>
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
constexpr uint8_t gen2Hex = 3;
constexpr uint8_t gen3p5 = 4;

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

// Byte offsets within a decoded hex-file record line
namespace RecordField
{
inline constexpr size_t Type = 0;
inline constexpr size_t Length = 1;
inline constexpr size_t Address = 2;
inline constexpr size_t Command = 3;
inline constexpr size_t DataStart = 4;
} // namespace RecordField

constexpr size_t headerSize = 2;
constexpr size_t addressSize = 1;
constexpr size_t commandSize = 1;
constexpr size_t pecSize = 1;

constexpr size_t configIdOffset = 0;

constexpr uint8_t defaultBufferSize = 16;
constexpr uint8_t programBufferSize = 32;

constexpr uint8_t zeroByteLen = 0;
constexpr uint8_t oneByteLen = 1;
constexpr uint8_t threeByteLen = 3;
constexpr uint8_t fourByteLen = 4;

// RAA Gen2
constexpr uint8_t gen2RegProgStatus = 0x07;
constexpr uint8_t gen2RegCRC = 0x3F;
constexpr uint8_t gen2RegRemainginWrites = 0xC2;

constexpr uint32_t gen2RevMin = 0x02000003;

constexpr uint8_t hexFileRev = 0x00;
constexpr uint16_t gen2FileHead = 6;
constexpr uint16_t gen2CRC = 600 - gen2FileHead;

// RAA Gen3p5
constexpr uint8_t regGen3p5ProgStatus = 0x83;
constexpr uint8_t gen3p5RegCRC = 0xF8;
constexpr uint8_t gen3p5HWRevMax = 0x06;
constexpr uint8_t gen3p5HWRevMin = 0x03;
constexpr uint16_t gen3p5cfgId = 3;
constexpr uint16_t gen3p5FileHead = 5;
constexpr uint16_t gen3p5CRC = 336 - gen3p5FileHead;
constexpr uint8_t gen3p5DeviceIdMin = 0xBA;
constexpr size_t gen3p5DeviceIdByteOffset = 2;

ISL69269::ISL69269(sdbusplus::async::context& ctx, uint16_t bus,
                   uint16_t address, Gen gen) :
    VoltageRegulator(ctx), i2cInterface(phosphor::i2c::I2C(bus, address)),
    generation(gen)
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
    if (reg == nullptr || resp == nullptr)
    {
        error("dmaReadWrite invalid input");
        co_return false;
    }

    uint8_t tbuf[defaultBufferSize] = {0};
    uint8_t tlen = threeByteLen;
    uint8_t rbuf[defaultBufferSize] = {0};
    uint8_t rlen = zeroByteLen;

    tbuf[0] = regDMAAddr;
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

    tbuf[0] = regDMAData;
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

    tbuf[0] =
        (generation == Gen::Gen2) ? gen2RegRemainginWrites : regRemainginWrites;
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
    if (generation == Gen::Gen2)
    {
        *mode = gen2Hex;
        co_return true;
    }
    else if (generation == Gen::Gen3p5)
    {
        uint32_t devID = 0;
        if (!(co_await getDeviceId(&devID)))
        {
            error("program failed at getDeviceId");
            co_return false;
        }
        devID = (devID >> 8) & 0xFF;

        if (devID >= gen3p5DeviceIdMin)
        {
            *mode = gen3p5;
        }
        co_return true;
    }

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
    if (deviceId == nullptr)
    {
        error("getDeviceId invalid input");
        co_return false;
    }

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
    if (revision == nullptr)
    {
        error("getDeviceRevision invalid input");
        co_return false;
    }

    uint8_t tbuf[defaultBufferSize] = {0};
    uint8_t tlen = oneByteLen;
    uint8_t rbuf[defaultBufferSize] = {0};
    uint8_t rlen = deviceRevisionLen + 1;

    tbuf[0] = pmBusDeviceRev;
    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    if (!(co_await i2cInterface.sendReceive(tbuf, tlen, rbuf, rlen)))
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    {
        error("getDeviceRevision failed with sendreceive");
        co_return false;
    }

    if (mode == gen3Legacy || mode == gen3p5)
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

    switch (generation)
    {
        case Gen::Gen2:
            tbuf[0] = gen2RegCRC;
            break;
        case Gen::Gen3p5:
            tbuf[0] = gen3p5RegCRC;
            break;
        default:
            tbuf[0] = regCRC;
            break;
    }

    if (!(co_await dmaReadWrite(tbuf, rbuf)))
    {
        error("getCRC failed");
        co_return false;
    }
    std::memcpy(sum, rbuf, sizeof(uint32_t));

    co_return true;
}

bool ISL69269::decodeHexLine(std::string_view rawLine,
                             std::span<uint8_t> sepLineOut,
                             size_t& decodedByteCount)
{
    decodedByteCount = 0;

    if (rawLine.ends_with('\r'))
    {
        // Remove '\r' when the line ending is CRLF.
        rawLine.remove_suffix(1);
    }

    if (rawLine.size() % 2 != 0)
    {
        error("parseImage failed. Odd number of hex characters.");
        return false;
    }

    if (rawLine.size() / 2 > sepLineOut.size())
    {
        error("parseImage failed. line length > {MAX}", "MAX",
              sepLineOut.size() * 2);
        return false;
    }

    for (; decodedByteCount * 2 < rawLine.size(); decodedByteCount++)
    {
        const size_t charIndex = decodedByteCount * 2;
        auto result =
            std::from_chars(&rawLine[charIndex], &rawLine[charIndex] + 2,
                            sepLineOut[decodedByteCount], 16);
        if (result.ec != std::errc{} || result.ptr != &rawLine[charIndex] + 2)
        {
            error("parseImage failed. Invalid hex digit in record line.");
            return false;
        }
    }

    return true;
}

bool ISL69269::decodeRecordLine(std::string_view rawLine, uint8_t& type,
                                Data& record)
{
    constexpr size_t maxDecodedBytes = 20;
    uint8_t sepLine[maxDecodedBytes] = {0};

    size_t decodedByteCount = 0;
    if (!decodeHexLine(rawLine, sepLine, decodedByteCount))
    {
        return false;
    }

    type = sepLine[RecordField::Type];
    const uint8_t payloadSize = sepLine[RecordField::Length];

    if (decodedByteCount != headerSize + payloadSize ||
        payloadSize < addressSize + commandSize + pecSize)
    {
        error(
            "parseImage failed. Record line length does not match declared size.");
        return false;
    }

    // According to documentation:
    // 00 05 C2 E7 08 00 F6
    //  |  |  |  |  |  |  |
    //  |  |  |  |  |  |  - Packet Error Code (CRC8)
    //  |  |  |  |  -  - Data (N bytes)
    //  |  |  |  - Command Code
    //  |  |  - Address
    //  |  - Size of data (including Addr, Cmd, CRC8)
    //  - Line type (0x00 - Data, 0x49 header information)
    const size_t dataSize = payloadSize - addressSize - commandSize - pecSize;
    record.len = dataSize + commandSize;
    record.addr = sepLine[RecordField::Address];
    record.cmd = sepLine[RecordField::Command];
    record.pec = sepLine[RecordField::DataStart + dataSize];
    // record.data holds Address, Command, and the Data payload.
    std::memcpy(record.data, sepLine + RecordField::Address,
                record.len + addressSize);

    return true;
}

bool ISL69269::handleHeaderRecord(const Data& record)
{
    if (record.len < commandSize + sizeof(configuration.devIdExp))
    {
        error("parseImage failed. Header record line too short.");
        return false;
    }

    // Data payload only, skipping Address and Command.
    const uint8_t* data = record.data + addressSize + commandSize;

    if (record.cmd == pmBusDeviceId)
    {
        shiftLeftFromMSB(data, &configuration.devIdExp);
        debug("device id from configuration: {ID}", "ID", lg2::hex,
              configuration.devIdExp);

        if (generation == Gen::Gen3p5 &&
            data[gen3p5DeviceIdByteOffset] >= gen3p5DeviceIdMin)
        {
            debug("Gen3p5 hex file format recognized");
            configuration.mode = gen3p5;
        }
    }
    else if (record.cmd == pmBusDeviceRev)
    {
        shiftLeftFromMSB(data, &configuration.devRevExp);
        debug("device revision from config: {ID}", "ID", lg2::hex,
              configuration.devRevExp);

        if (generation == Gen::Gen3)
        {
            // According to programming guide:
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
    else if (record.cmd == hexFileRev)
    {
        debug("Gen2 hex file format recognized");
        configuration.mode = gen2Hex;
    }

    return true;
}

bool ISL69269::handleDataRecord(int dcnt, const Data& record)
{
    // Data payload only, skipping Address and Command.
    const uint8_t* data = record.data + addressSize + commandSize;

    switch (dcnt)
    {
        case cfgId:
            if (configuration.mode != gen3p5)
            {
                configuration.cfgId = data[configIdOffset] & 0x0F;
                debug("Config ID: {ID}", "ID", lg2::hex, configuration.cfgId);
            }
            break;
        case gen3p5cfgId:
            if (configuration.mode == gen3p5)
            {
                configuration.cfgId = data[configIdOffset];
                debug("Config ID: {ID}", "ID", lg2::hex, configuration.cfgId);
            }
            break;
        case gen3LegacyCRC:
            if (configuration.mode == gen3Legacy)
            {
                std::memcpy(&configuration.crcExp, data, checksumLen);
                debug("Config Legacy CRC: {CRC}", "CRC", lg2::hex,
                      configuration.crcExp);
            }
            break;
        case gen3ProductionCRC:
            if (configuration.mode == gen3Production)
            {
                std::memcpy(&configuration.crcExp, data, checksumLen);
                debug("Config Production CRC: {CRC}", "CRC", lg2::hex,
                      configuration.crcExp);
            }
            break;
        case gen2CRC:
            if (configuration.mode == gen2Hex)
            {
                std::memcpy(&configuration.crcExp, data, checksumLen);
                debug("Config Gen2 CRC: {CRC}", "CRC", lg2::hex,
                      configuration.crcExp);
            }
            break;
        case gen3p5CRC:
            if (configuration.mode == gen3p5)
            {
                std::memcpy(&configuration.crcExp, data, checksumLen);
                debug("Config Gen3p5 CRC: {CRC}", "CRC", lg2::hex,
                      configuration.crcExp);
            }
            break;
    }

    return true;
}

bool ISL69269::parseImage(const uint8_t* image, size_t imageSize)
{
    size_t nextLineStart = 0;
    int dcnt = 0;

    for (size_t imageIndex = 0; imageIndex < imageSize; imageIndex++)
    {
        if (image[imageIndex] == '\n')
        {
            std::string_view rawLine{
                reinterpret_cast<const char*>(image + nextLineStart),
                imageIndex - nextLineStart};

            uint8_t type = 0;
            Data record{};
            if (!decodeRecordLine(rawLine, type, record))
            {
                return false;
            }

            if (type == recordTypeHeader)
            {
                if (!handleHeaderRecord(record))
                {
                    return false;
                }
            }
            else if (type == recordTypeData)
            {
                if (static_cast<size_t>(dcnt) >= maxDataRecords)
                {
                    error(
                        "parseImage failed. Image contains too many data records.");
                    return false;
                }
                if (!handleDataRecord(dcnt, record))
                {
                    return false;
                }
                configuration.pData[dcnt] = record;
                dcnt++;
            }
            else
            {
                error("parseImage failed. Unknown recordType");
                return false;
            }

            nextLineStart = imageIndex + 1;
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
                        configuration.pData[i].len + addressSize);
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
        std::memcpy(tbuf, configuration.pData[i].data + addressSize,
                    configuration.pData[i].len);

        if (!(co_await i2cInterface.sendReceive(
                tbuf, configuration.pData[i].len, rbuf, rlen)))
        {
            error("program failed at writing data to voltage regulator");
        }
    }

    if (!(co_await getProgStatus()))
    {
        error("program failed at getProgStatus");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> ISL69269::getProgStatus()
{
    uint8_t tbuf[programBufferSize] = {0};
    uint8_t rbuf[programBufferSize] = {0};
    int retry = 3;

    if (generation == Gen::Gen2)
    {
        tbuf[0] = gen2RegProgStatus;
        tbuf[1] = gen2RegProgStatus;
    }
    else if (generation == Gen::Gen3p5)
    {
        tbuf[0] = regGen3p5ProgStatus;
        tbuf[1] = 0x00;
    }
    else
    {
        tbuf[0] = regProgStatus;
        tbuf[1] = 0x00;
    }

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
            debug("Programming successful");
            break;
        }
        if (--retry == 0)
        {
            if ((!(rbuf[1] & 0x1)) || (rbuf[1] & 0x2))
            {
                error("programming the device failed");
            }
            if (!(rbuf[1] & 0x4))
            {
                error(
                    "HEX file contains more configurations than are available");
            }
            if (!(rbuf[1] & 0x8))
            {
                error(
                    "A CRC mismatch exists within the configuration data. Programming failed before  TP banks are consumed");
            }
            if (!(rbuf[1] & 0x10))
            {
                error(
                    "CRC check fails on the OTP memory. Programming fails after OTP banks are consumed");
            }
            if (!(rbuf[1] & 0x20))
            {
                error("Programming fails after OTP banks are consumed.");
            }

            error("failed to program the device after exceeding retries");
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

    debug("Restore configuration ID: {ID}", "ID", lg2::hex,
          configuration.cfgId);

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
        case gen2Hex:
            if (devRev >= gen2RevMin)
            {
                debug("Gen2 mode revision checks out");
            }
            else
            {
                error("revision requirements for Gen2 device not fulfilled");
            }
            break;
        case gen3p5:
            if (((devRev >> 24) >= gen3p5HWRevMin) &&
                ((devRev >> 24) <= gen3p5HWRevMax))
            {
                debug("Gen3p5 revision checks out");
            }
            else
            {
                error("revision requirements for Gen3p5 device not fulfilled");
            }
            break;
    }

    if (!(co_await getCRC(&crc)))
    {
        error("program failed at getCRC");
        co_return false;
    }

    debug("CRC from device: {CRC}", "CRC", lg2::hex, crc);
    debug("CRC from config: {CRC}", "CRC", lg2::hex, configuration.crcExp);

    if (crc == configuration.crcExp)
    {
        error("program failed with same CRC value at device and configuration");
        co_return false;
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
    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    if (!(co_await program()))
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    {
        error("programming ISL69269 failed");
        co_return false;
    }

    co_return true;
}

} // namespace phosphor::software::VR
