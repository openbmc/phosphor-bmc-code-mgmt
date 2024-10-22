#include "xdpe1x2xx.hpp"

#include "common/include/i2c/i2c.hpp"

#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <cstdio>

#define REMAINING_TIMES(x, y) (((((x)[1]) << 8) | ((x)[0])) / (y))

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

const uint32_t CRC32Poly = 0xEDB88320;
const int VRResetDelay = 500000;

enum RevisionCode
{
    REV_A = 0x00,
    REV_B,
    REV_C,
    REV_D,
};

enum ProductID
{
    ProductIDXDPE15284 = 0x8A,
    ProductIDXDPE19283 = 0x95,
};

const uint32_t PMBusICDeviceID = 0xAD;
const uint32_t PMBusSTLCml = 0x7E;
const uint32_t IFXICDeviceIDLen = 2;
const uint32_t IFXMFRAHBAddr = 0xCE;
const uint32_t IFXMFRRegWrite = 0xDE;
const uint32_t IFXMFRFwCmdData = 0xDF;
const uint32_t IFXMFRFwCmd = 0xFE;
const uint32_t MFRFwCmdReset = 0x0e;
const uint32_t MFRFwCmdRmng = 0x10;
const uint32_t MFRFwCmdOTPConfSTO = 0x11;
const uint32_t MFRFwCmdOTPFileInvd = 0x12;
const uint32_t MFRFwCmdGetCRC = 0x2D;
const int XDPE15284CConfSize = 1344;
const int XDPE19283BConfSize = 1416;
const uint32_t VRWarnRemaining = 3;
const uint32_t SectTrim = 0x02;

const char* const AddressField = "PMBus Address :";
const char* const ChecksumField = "Checksum :";
const char* const DataStartTag = "Configuration Data]";
const char* const DataEndTag = "[End Configuration Data]";
const char* const DataComment = "//";
const char* const DataXV = "XV";

XDPE1X2XX::XDPE1X2XX(sdbusplus::async::context& ctx, uint16_t bus,
                     uint16_t address) :
    VoltageRegulator(ctx),
    i2cInterface(std::make_unique<phosphor::i2c::I2C>(bus, address))
{}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> XDPE1X2XX::getDeviceId(uint8_t* deviceID)
// NOLINTEND(readability-static-accessed-through-instance)
{
    bool ret = false;
    uint8_t tbuf[16] = {0};
    tbuf[0] = PMBusICDeviceID;
    tbuf[1] = 2;
    uint8_t tSize = 1;
    uint8_t rbuf[16] = {0};
    uint8_t rSize = IFXICDeviceIDLen + 1;

    ret = co_await this->i2cInterface->sendReceive(tbuf, tSize, rbuf, rSize);
    if (!ret)
    {
        error("Failed to get device ID");
        co_return false;
    }

    std::memcpy(deviceID, &rbuf[1], IFXICDeviceIDLen);

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> XDPE1X2XX::mfrFWcmd(uint8_t cmd, uint8_t* data,
                                                 uint8_t* resp)
// NOLINTEND(readability-static-accessed-through-instance)
{
    bool ret = false;
    uint8_t tBuf[16] = {0};
    uint8_t rBuf[16] = {0};
    uint8_t tSize = 0;
    uint8_t rSize = 0;

    if (data)
    {
        tBuf[0] = IFXMFRFwCmdData;
        tBuf[1] = 4; // Block write 4 bytes
        tSize = 6;
        std::memcpy(&tBuf[2], data, 4);
        ret =
            co_await this->i2cInterface->sendReceive(tBuf, tSize, rBuf, rSize);
        if (!ret)
        {
            error("Failed to send MFR command: {CMD}", "CMD",
                  std::string("IFXMFRFwCmdDAta"));
            co_return false;
        }
    }

    co_await sdbusplus::async::sleep_for(ctx, std::chrono::microseconds(300));

    tBuf[0] = IFXMFRFwCmd;
    tBuf[1] = cmd;
    tSize = 2;
    rSize = 0;
    ret = co_await this->i2cInterface->sendReceive(tBuf, tSize, rBuf, rSize);
    if (!ret)
    {
        error("Failed to send MFR command: {CMD}", "CMD",
              std::string("IFXMFRFwCmd"));
        co_return false;
    }

    co_await sdbusplus::async::sleep_for(ctx, std::chrono::microseconds(20000));

    if (resp)
    {
        tBuf[0] = IFXMFRFwCmdData;
        tSize = 1;
        rSize = 6;
        ret =
            co_await this->i2cInterface->sendReceive(tBuf, tSize, rBuf, rSize);
        if (!ret)
        {
            error("Failed to send MFR command: {CMD}", "CMD",
                  std::string("IFXMFRFwCmdData"));
            co_return false;
        }
        if (rBuf[0] != 4)
        {
            error(
                "Failed to receive MFR response with unexpected response size");
            co_return false;
        }
        std::memcpy(resp, rBuf + 1, 4);
    }

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> XDPE1X2XX::getRemainingWrites(uint8_t* remain)
// NOLINTEND(readability-static-accessed-through-instance)
{
    bool ret = false;
    uint8_t tBuf[16] = {0};
    uint8_t rBuf[16] = {0};
    uint8_t devId[2] = {0};

    ret = co_await this->mfrFWcmd(MFRFwCmdRmng, tBuf, rBuf);
    if (!ret)
    {
        error("Failed to request remaining writes");
        co_return false;
    }

    ret = co_await this->getDeviceId(devId);
    if (!ret)
    {
        error("Failed to request device ID for remaining writes");
        co_return false;
    }

    int configSize = getConfigSize(devId[1], devId[0]);
    if (configSize < 0)
    {
        error("Failed to request valid configuration size");
        co_return false;
    }

    *remain = REMAINING_TIMES(rBuf, configSize);

    co_return 0;
}

int XDPE1X2XX::getConfigSize(uint8_t deviceId, uint8_t revision)
{
    int size = -1;

    switch (deviceId)
    {
        case ProductIDXDPE19283:
            if (revision == REV_B)
            {
                size = XDPE19283BConfSize;
            }
            break;
        case ProductIDXDPE15284:
            size = XDPE15284CConfSize;
            break;
        default:
            error(
                "Failed to get configuration size of {DEVID} with revision {REV}",
                "DEVID", deviceId, "REV", revision);
            return -1;
    }

    return size;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> XDPE1X2XX::getCRC(uint32_t* checksum)
// NOLINTEND(readability-static-accessed-through-instance)
{
    uint8_t tBuf[16] = {0};
    uint8_t rBuf[16] = {0};

    bool ret = co_await this->mfrFWcmd(MFRFwCmdGetCRC, tBuf, rBuf);
    if (!ret)
    {
        error("Failed to get CRC value");
        co_return false;
    }

    *checksum = (static_cast<uint32_t>(rBuf[3]) << 24) |
                (static_cast<uint32_t>(rBuf[2]) << 16) |
                (static_cast<uint32_t>(rBuf[1]) << 8) |
                (static_cast<uint32_t>(rBuf[0]));

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> XDPE1X2XX::program(bool force)
// NOLINTEND(readability-static-accessed-through-instance)

{
    bool ret = false;
    uint8_t tBuf[16] = {0};
    uint8_t rBuf[16] = {0};
    uint8_t remain = 0;
    uint32_t sum = 0;
    int size = 0;

    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    ret = co_await getCRC(&sum);
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    if (!ret)
    {
        error("Failed to program the VR");
        co_return -1;
    }

    if (!force && (sum == configuration.sumExp))
    {
        error("Failed to program the VR - CRC value are equal with no force");
        co_return -1;
    }

    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    ret = co_await this->getRemainingWrites(&remain);
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    if (!ret)
    {
        error("Failed to program the VR - unable to obtain remaing writes");
        co_return -1;
    }

    if (!remain)
    {
        error("Failed to program the VR - no remaining write cycles left");
        co_return -1;
    }

    if (!force && (remain <= VRWarnRemaining))
    {
        error(
            "Failed to program the VR - {REMAIN} remaining writes left and not force",
            "REMAIN", remain);
        co_return -1;
    }

    // Added reprogramming of the entire configuration file.
    // Except for the trim section, all other data will be replaced.
    // If the old sections are not invalidated in OTP, they can affect the CRC
    // calculation.

    tBuf[0] = 0xfe;
    tBuf[1] = 0xfe;
    tBuf[2] = 0x00;
    tBuf[3] = 0x00;

    ret = co_await this->mfrFWcmd(MFRFwCmdOTPFileInvd, tBuf, NULL);
    if (!ret)
    {
        error("Failed to program the VR - Invalidation of currect FW");
        co_return ret;
    }

    co_await sdbusplus::async::sleep_for(ctx,
                                         std::chrono::microseconds(500000));

    for (int i = 0; i < configuration.sectCnt; i++)
    {
        struct configSect* sect = &configuration.section[i];
        if (sect == NULL)
        {
            error(
                "Failed to program the VR - unexpected NULL section in config");
            ret = -1;
            break;
        }

        if ((i <= 0) || (sect->type != configuration.section[i - 1].type))
        {
            tBuf[0] = PMBusSTLCml;
            tBuf[1] = 0x1;
            uint8_t tSize = 2;
            uint8_t rSize = 0;
            ret = co_await this->i2cInterface->sendReceive(tBuf, tSize, rBuf,
                                                           rSize);
            if (!ret)
            {
                error("Failed to program the VR on sendReceive {CMD}", "CMD",
                      std::string("PMBusSTLCml"));
                break;
            }

            tBuf[0] = sect->type;
            tBuf[1] = 0x00;
            tBuf[2] = 0x00;
            tBuf[3] = 0x00;

            ret = co_await this->mfrFWcmd(MFRFwCmdOTPFileInvd, tBuf, NULL);
            if (!ret)
            {
                error("Failed to program VR on mfrFWCmd on {CMD}", "CMD",
                      std::string("MFRFwCmdOTPFileInvd"));
                break;
            }

            co_await sdbusplus::async::sleep_for(
                ctx, std::chrono::microseconds(10000)); // Write delay

            tBuf[0] = IFXMFRAHBAddr;
            tBuf[1] = 4;
            tBuf[2] = 0x00;
            tBuf[3] = 0xe0;
            tBuf[4] = 0x05;
            tBuf[5] = 0x20;
            tSize = 6;
            rSize = 0;

            ret = co_await this->i2cInterface->sendReceive(tBuf, tSize, rBuf,
                                                           rSize);
            if (!ret)
            {
                error("Failed to program VR on sendReceive on {CMD}", "CMD",
                      std::string("IFXMFRAHBAddr"));
                break;
            }

            co_await sdbusplus::async::sleep_for(
                ctx, std::chrono::microseconds(10000));
            size = 0;
        }

        // programm into scratchpad
        for (int j = 0; j < sect->dataCnt; j++)
        {
            tBuf[0] = IFXMFRRegWrite;
            tBuf[1] = 4;
            uint8_t tSize = 6;
            uint8_t rSize = 0;
            memcpy(&tBuf[2], &sect->data[j], 4);
            ret = co_await this->i2cInterface->sendReceive(tBuf, tSize, rBuf,
                                                           rSize);
            if (!ret)
            {
                error("Failed to program the VR on sendReceive {CMD}", "CMD",
                      std::string("IFXMFRRegWrite"));
                break;
            }
            co_await sdbusplus::async::sleep_for(
                ctx, std::chrono::microseconds(10000));
        }
        if (ret)
        {
            break;
        }

        size += sect->dataCnt * 4;
        if ((i + 1 >= configuration.sectCnt) ||
            (sect->type != configuration.section[i + 1].type))
        {
            // Upload to scratchpad
            std::memcpy(tBuf, &size, 2);
            tBuf[2] = 0x00;
            tBuf[3] = 0x00;
            bool ret = co_await this->mfrFWcmd(MFRFwCmdOTPConfSTO, tBuf, NULL);
            if (ret)
            {
                error("Failed to program the VR on mfrFWcmd {CMD}", "CMD",
                      std::string("MFRFwCmdOTPConfSTO"));
                break;
            }

            // wait for programming soak (2ms/byte, at least 200ms)
            // ex: Config (604 bytes): (604 / 50) + 2 = 14 (1400 ms)
            size = (size / 50) + 2;
            for (int j = 0; j < size; j++)
            {
                co_await sdbusplus::async::sleep_for(
                    ctx, std::chrono::microseconds(100000));
            }

            tBuf[0] = PMBusSTLCml;
            uint8_t tSize = 1;
            uint8_t rSize = 1;
            ret = co_await this->i2cInterface->sendReceive(rBuf, tSize, tBuf,
                                                           rSize);
            if (!ret)
            {
                error("Failed to program VR on sendReceive {CMD}", "CMD",
                      std::string("PMBusSTLCml"));
                break;
            }
            if (rBuf[0] & 0x01)
            {
                error("Failed to program VR - response code invalid");
                break;
            }
        }
    }

    if (!ret)
    {
        co_return false;
    }

    co_return true;
}

int XDPE1X2XX::lineSplit(char** dest, char* src, char* delim)
{
    char* s = strtok(src, delim);
    int size = 0;
    int maxSz = 5;

    while (s)
    {
        *dest++ = s;
        if ((++size) >= maxSz)
        {
            break;
        }
        s = strtok(NULL, delim);
    }

    return size;
}

int XDPE1X2XX::parseImage(const uint8_t* image, size_t image_size)
{
    size_t lenEndTag = strlen(DataEndTag);
    size_t lenStartTag = strlen(DataStartTag);
    size_t lenComment = strlen(DataComment);
    size_t lenXV = strlen(DataXV);
    size_t start = 0;
    const int maxLineLength = 40;
    char line[maxLineLength];
    char* token = NULL;
    bool isData = false;
    char delim = ' ';
    uint16_t offset;
    uint8_t sectType = 0x0;
    uint32_t dWord;
    int dataCnt = 0;
    int sectIndex = -1;

    for (size_t i = 0; i < image_size; i++)
    {
        if (image[i] == '\n')
        {
            std::memcpy(line, image + start, i - start);
            if (!strncmp(line, DataComment, lenComment))
            {
                token = line + lenComment;
                if (!strncmp(token, DataXV, lenXV))
                {
                    debug("Parsing: {OBJ}", "OBJ",
                          reinterpret_cast<const char*>(line));
                }
                start = i + 1;
                continue;
            }
            if (!strncmp(line, DataEndTag, lenEndTag))
            {
                debug("Parsing: {OBJ}", "OBJ",
                      reinterpret_cast<const char*>(line));
                // NOLINTBEGIN(clang-analyzer-deadcode.DeadStores)
                start = i + 1;
                // NOLINTEND(clang-analyzer-deadcode.DeadStores)
                break;
            }
            else if (isData)
            {
                char* tokenList[8] = {0};
                int tokenSize = lineSplit(tokenList, line, &delim);
                if (tokenSize < 1)
                {
                    start = i + 1;
                    continue;
                }

                offset = (uint16_t)strtol(tokenList[0], NULL, 16);
                if (sectType == SectTrim && offset != 0x0)
                {
                    continue;
                }

                for (int i = 1; i < tokenSize; i++)
                {
                    dWord = (uint32_t)strtol(tokenList[i], NULL, 16);
                    if ((offset == 0x0) && (i == 1))
                    {
                        sectType = (uint8_t)dWord;
                        if (sectType == SectTrim)
                        {
                            break;
                        }
                        if ((++sectIndex) >= MaxSectCnt)
                        {
                            return -1;
                        }

                        configuration.section[sectIndex].type = sectType;
                        configuration.sectCnt = sectIndex + 1;
                        dataCnt = 0;
                    }

                    if (dataCnt >= MaxSectDataCnt)
                    {
                        return -1;
                    }

                    configuration.section[sectIndex].data[dataCnt++] = dWord;
                    configuration.section[sectIndex].dataCnt = dataCnt;
                    configuration.totalCnt++;
                }
            }
            else
            {
                if ((token = strstr(line, AddressField)) != NULL)
                {
                    if ((token = strstr(token, "0x")) != NULL)
                    {
                        configuration.addr =
                            (uint8_t)(strtoul(token, NULL, 16) << 1);
                    }
                }
                else if ((token = strstr(line, ChecksumField)) != NULL)
                {
                    if ((token = strstr(token, "0x")) != NULL)
                    {
                        configuration.sumExp =
                            (uint32_t)strtoul(token, NULL, 16);
                    }
                }
                else if (!strncmp(line, DataStartTag, lenStartTag))
                {
                    isData = true;
                    start = i + 1;
                    continue;
                }
                else
                {
                    start = i + 1;
                    continue;
                }
            }
            start = i + 1;
        }
    }

    return 0;
}

int XDPE1X2XX::checkImage()
{
    uint8_t i;
    uint32_t crc;
    uint32_t sum = 0;

    for (i = 0; i < configuration.sectCnt; i++)
    {
        struct configSect* sect = &configuration.section[i];
        if (sect == NULL)
        {
            error("Failed to check image - unexpected NULL section");
            return -1;
        }

        crc = calcCRC32(&sect->data[2], 2);
        if (crc != sect->data[2])
        {
            error("Failed to check image - first CRC value mismatch");
            return -1;
        }
        sum += crc;

        // check CRC of section data
        crc = calcCRC32(&sect->data[3], sect->dataCnt - 4);
        if (crc != sect->data[sect->dataCnt - 1])
        {
            error("Failed to check image - second CRC value mismatch");
            return -1;
        }
        sum += crc;
    }

    if (sum != configuration.sumExp)
    {
        error("Failed to check image - third CRC value mismatch");
        return -1;
    }

    return 0;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> XDPE1X2XX::verifyImage(const uint8_t* image,
                                                    size_t imageSize)
// NOLINTEND(readability-static-accessed-through-instance)
{
    if (parseImage(image, imageSize) < 0)
    {
        error("Failed to update firmware on parsing Image");
        co_return false;
    }

    if (checkImage() < 0)
    {
        error("Failed to update firmware on check image");
        co_return false;
    }

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> XDPE1X2XX::updateFirmware(bool force)
// NOLINTEND(readability-static-accessed-through-instance)
{
    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    bool ret = co_await program(force);
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    if (!ret)
    {
        error("Failed to update firmware on program");
        co_return false;
    }

    // Reset the configuration
    struct xdpe1x2xxConfig cfg;
    configuration = cfg;
    ;

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> XDPE1X2XX::reset()
// NOLINTEND(readability-static-accessed-through-instance)
{
    bool ret = co_await mfrFWcmd(MFRFwCmdReset, NULL, NULL);
    if (!ret)
    {
        error("Failed to reset the VR");
        co_return false;
    }

    co_await sdbusplus::async::sleep_for(
        ctx, std::chrono::microseconds(VRResetDelay));

    co_return true;
}

uint32_t XDPE1X2XX::calcCRC32(const uint32_t* data, int len)
{
    uint32_t crc = 0xFFFFFFFF;
    int i;
    int b;

    if (data == NULL)
    {
        return 0;
    }

    for (i = 0; i < len; i++)
    {
        crc ^= data[i];

        for (b = 0; b < 32; b++)
        {
            if (crc & 0x1)
            {
                crc = (crc >> 1) ^ CRC32Poly; // lsb-first
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}

bool XDPE1X2XX::forcedUpdateAllowed()
{
    return true;
}

} // namespace phosphor::software::VR
