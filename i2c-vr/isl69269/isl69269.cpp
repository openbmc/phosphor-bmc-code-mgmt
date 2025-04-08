#include "isl69269.hpp"

#include "common/include/i2c/i2c.hpp"

#include <phosphor-logging/lg2.hpp>

#include <string>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

const uint8_t RAARegProgStatus = 0x7E;
const uint8_t RAARegHexModeCFG0 = 0x87;
const uint8_t RAARegCRC = 0x94;
const uint8_t RAARegHexModeCFG1 = 0xBD;
const uint8_t RAARegDMAData = 0xC5;
const uint8_t RAARegDMAAddr = 0xC7;
const uint8_t RAARegRestoreCfg = 0xF2;

const uint8_t RAARegRemainginWrites = 0x35;

const uint8_t RAAGen3SWRevMin = 0x06;
const uint8_t RAADeviceIdLength = 4;

const uint8_t RAAGen3Legacy = 1;
const uint8_t RAAGen3Production = 2;

const uint16_t RAACfgId = 7;
const uint16_t RAAGen3FileHead = 5;
const uint16_t RAAGen3LegacyCRC = 276 - RAAGen3FileHead;
const uint16_t RAAGen3ProductionCRC = 290 - RAAGen3FileHead;
const uint8_t RAAChecksumLen = 4;
const uint8_t RAADeviceRevisionLen = 4;

// Common PMBus Command codes
const uint8_t PMBusDeviceId = 0xAD;
const uint8_t PMBusDeviceRev = 0xAE;

// Config file constants
const char RecordTypeData = 0x00;
const char RecordTypeHeader = 0x49;

ISL69269::ISL69269(sdbusplus::async::context& ctx, uint16_t bus,
                   uint16_t address) :
    VoltageRegulator(ctx), i2cInterface(phosphor::i2c::I2C(bus, address))
{}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> ISL69269::dmaReadWrite(uint8_t* reg, uint8_t* resp)
// NOLINTEND(readability-static-accessed-through-instance)
{
    uint8_t tbuf[16] = {0};
    uint8_t rbuf[16] = {0};

    tbuf[0] = RAARegDMAData;
    std::memcpy(&tbuf[1], reg, 2);

    if (!(co_await this->i2cInterface.sendReceive(tbuf, 3, rbuf, 0)))
    {
        error("dmaReadWrite failed with {CMD}", "CMD",
              std::string("RAA_REG_DMA_ADDR"));
        co_return false;
    }

    tbuf[0] = RAARegDMAAddr;
    if (!(co_await this->i2cInterface.sendReceive(tbuf, 1, resp, 4)))
    {
        error("dmaReadWrite failed with {CMD}", "CMD",
              std::string("RAA_REG_DMA_DATA"));
        co_return false;
    }

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> ISL69269::getRemainingWrites(uint8_t* remain)
// NOLINTEND(readability-static-accessed-through-instance)
{
    uint8_t tbuf[16] = {0};
    uint8_t rbuf[16] = {0};

    tbuf[0] = RAARegRemainginWrites;
    tbuf[1] = 0x00;
    if (!(co_await dmaReadWrite(tbuf, rbuf)))
    {
        error("getRemainingWrites failed");
        co_return false;
    }

    *remain = rbuf[0];
    co_return true;
}
// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> ISL69269::getHexMode(uint8_t* mode)
// NOLINTEND(readability-static-accessed-through-instance)
{
    uint8_t tbuf[16] = {0};
    uint8_t rbuf[16] = {0};

    tbuf[0] = RAARegHexModeCFG0;
    tbuf[1] = RAARegHexModeCFG1;
    if (!(co_await this->dmaReadWrite(tbuf, rbuf)))
    {
        error("getHexMode failed");
        co_return false;
    }

    *mode = (rbuf[0] == 0) ? RAAGen3Legacy : RAAGen3Production;

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> ISL69269::getDeviceId(uint32_t* deviceId)
// NOLINTEND(readability-static-accessed-through-instance)
{
    uint8_t tbuf[16] = {0};
    uint8_t tLen = 1;
    uint8_t rbuf[16] = {0};
    uint8_t rLen = RAADeviceIdLength + 1;

    tbuf[0] = PMBusDeviceId;

    if (!(co_await this->i2cInterface.sendReceive(tbuf, rLen, rbuf, tLen)))
    {
        error("getDeviceId failed");
        co_return false;
    }

    *deviceId = rbuf[1];

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> ISL69269::getDeviceRevision(uint32_t* revision)
// NOLINTEND(readability-static-accessed-through-instance)
{
    uint8_t tbuf[8] = {0};
    uint8_t rbuf[8] = {0};

    tbuf[0] = PMBusDeviceRev;
    if (!(co_await this->i2cInterface.sendReceive(tbuf, 1, rbuf,
                                                  RAADeviceRevisionLen + 1)))
    {
        error("getDeviceRevision failed with sendReceive");
    }

    if (this->mode == RAAGen3Legacy)
    {
        std::memcpy(revision, &rbuf[1], RAADeviceRevisionLen);
    }
    else
    {
        *revision = (static_cast<uint32_t>(rbuf[4]) << 24) |
                    (static_cast<uint32_t>(rbuf[3]) << 16) |
                    (static_cast<uint32_t>(rbuf[2]) << 8) |
                    (static_cast<uint32_t>(rbuf[1]));
    }

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> ISL69269::getCRC(uint32_t* sum)
// NOLINTEND(readability-static-accessed-through-instance)
{
    uint8_t tbuf[16] = {0};
    uint8_t rbuf[16] = {0};

    tbuf[0] = RAARegCRC;
    if (!(co_await this->dmaReadWrite(tbuf, rbuf)))
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
            uint32_t sepLine[32] = {0};

            size_t lineLen = i - nextLineStart;
            std::memcpy(line, image + nextLineStart, lineLen);
            // Process Record Type
            int k = 0;
            size_t j = 0;
            for (k = 0, j = 0; j < lineLen; k++, j += 2)
            {
                // Convert two chars into a array of single values
                std::memcpy(xdigit, &line[j], 2);
                sepLine[k] = std::strtol(xdigit, NULL, 16);
            }

            if (sepLine[0] == RecordTypeHeader)
            {
                if (sepLine[3] == PMBusDeviceId)
                {
                    configuration.devIdExp =
                        (static_cast<uint32_t>(sepLine[7]) << 24) |
                        (static_cast<uint32_t>(sepLine[6]) << 16) |
                        (static_cast<uint32_t>(sepLine[5]) << 8) |
                        (static_cast<uint32_t>(sepLine[4]));
                    debug("configuration: device id: {ID}", "ID", lg2::hex,
                          configuration.devIdExp);
                }
                else if (sepLine[3] == PMBusDeviceRev)
                {
                    configuration.devRevExp =
                        (static_cast<uint32_t>(sepLine[4]) << 24) |
                        (static_cast<uint32_t>(sepLine[5]) << 16) |
                        (static_cast<uint32_t>(sepLine[6]) << 8) |
                        (static_cast<uint32_t>(sepLine[7]));

                    debug("configuration: device id: {ID}", "ID", lg2::hex,
                          configuration.devRevExp);

                    // According to programming guide:
                    // If legacy hex file
                    // MSB device revision == 0x00 | 0x01
                    if (configuration.devRevExp < (RAAGen3SWRevMin << 24))
                    {
                        debug("Legacy hex file format recognized");
                        configuration.mode = RAAGen3Legacy;
                    }
                    else
                    {
                        debug("Production hex file format recognized");
                        configuration.mode = RAAGen3Production;
                    }
                }
            }
            else if (sepLine[0] == RecordTypeData)
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
                    case RAACfgId:
                        configuration.cfgId = sepLine[4] & 0x0F;
                        debug("Config ID: {ID}", "ID", configuration.cfgId);
                        break;
                    case RAAGen3LegacyCRC:
                        if (configuration.mode == RAAGen3Legacy)
                        {
                            std::memcpy(&configuration.crcExp, &sepLine[4],
                                        RAAChecksumLen);
                            debug("Config Legacy CRC: {CRC}", "CRC", lg2::hex,
                                  configuration.crcExp);
                        }
                        break;
                    case RAAGen3ProductionCRC:
                        if (configuration.mode == RAAGen3Production)
                        {
                            std::memcpy(&configuration.crcExp, &sepLine[4],
                                        RAAChecksumLen);
                            debug("Config Production CRC: {CRC}", "CRC",
                                  lg2::hex, configuration.crcExp);
                        }
                        break;
                }
                dcnt++;
            }
            else
            {
                error("parseImage failed. Unknown RecordType");
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

uint8_t ISL69269::calcCRC8(const uint8_t* data, uint8_t len)
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

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> ISL69269::program()
// NOLINTEND(readability-static-accessed-through-instance)
{
    uint8_t tbuf[32] = {0};
    uint8_t rbuf[32] = {0};

    for (int i = 0; i < configuration.wrCnt; i++)
    {
        tbuf[0] = configuration.pData[i].cmd;
        std::memcpy(tbuf + 1, &configuration.pData[i].data,
                    configuration.pData[i].len - 1);

        if (!(co_await this->i2cInterface.sendReceive(
                tbuf, configuration.pData[i].len, rbuf, 0)))
        {
            error("program failed at writing data to voltage regulator");
        }
    }

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> ISL69269::getProgStatus()
// NOLINTEND(readability-static-accessed-through-instance)
{
    uint8_t tbuf[32] = {0};
    uint8_t rbuf[32] = {0};
    int retry = 3;

    tbuf[0] = RAARegProgStatus;
    tbuf[1] = 0x00;

    do
    {
        if (!(co_await this->dmaReadWrite(tbuf, rbuf)))
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

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> ISL69269::restoreCfg()
// NOLINTEND(readability-static-accessed-through-instance)
{
    uint8_t tbuf[8] = {0};
    uint8_t rbuf[8] = {0};

    tbuf[0] = RAARegRestoreCfg;
    tbuf[1] = configuration.cfgId;

    if (!(co_await this->dmaReadWrite(tbuf, rbuf)))
    {
        error("restoreCfg failed at dmaReadWrite");
        co_return false;
    }

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> ISL69269::verifyImage(const uint8_t* image,
                                                   size_t imageSize)
// NOLINTEND(readability-static-accessed-through-instance)
{
    uint8_t mode = 0xFF;
    uint8_t remain = 0;
    uint32_t devID = 0;
    uint32_t devRev = 0;
    uint32_t crc = 0;

    if (!(co_await this->getHexMode(&mode)))
    {
        error("program failed at getHexMode");
        co_return false;
    }

    if (!this->parseImage(image, imageSize))
    {
        error("verifyImage failed at parseImage");
        co_return false;
    }

    if (!this->checkImage())
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

    if (!(co_await this->getRemainingWrites(&remain)))
    {
        error("program failed at getRemainingWrites");
        co_return false;
    }

    if (!remain)
    {
        error("program failed with no remaining writes left on device");
        co_return false;
    }

    if (!(co_await this->getDeviceId(&devID)))
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

    if (!(co_await this->getDeviceRevision(&devRev)))
    {
        error("program failed at getDeviceRevision");
        co_return false;
    }

    if (devRev != configuration.devRevExp)
    {
        error(
            "program failed with not matching revision of device and configuration");
        co_return false;
    }

    if (!(co_await this->getCRC(&crc)))
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
// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> ISL69269::reset()
// NOLINTEND(readability-static-accessed-through-instance)
{
    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    if (!(co_await this->getProgStatus()))
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    {
        error("reset failed at getProgStatus");
        co_return false;
    }

    if (!(co_await this->restoreCfg()))
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

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> ISL69269::updateFirmware(bool force)
// NOLINTEND(readability-static-accessed-through-instance)
{
    (void)force;
    if (!(co_await this->program()))
    {
        error("programming ISL69269 failed");
        co_return false;
    }

    co_return true;
}

} // namespace phosphor::software::VR
