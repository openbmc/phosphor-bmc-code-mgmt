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
constexpr uint16_t VRResetDelay = 500;

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

constexpr uint8_t PMBusICDeviceID = 0xAD;
constexpr uint8_t PMBusSTLCml = 0x7E;
constexpr uint8_t IFXICDeviceIDLen = 2;
constexpr uint8_t IFXMFRAHBAddr = 0xCE;
constexpr uint8_t IFXMFRRegWrite = 0xDE;
constexpr uint8_t IFXMFRFwCmdData = 0xFD;
constexpr uint8_t IFXMFRFwCmd = 0xFE;
constexpr uint8_t MFRFwCmdReset = 0x0e;
constexpr uint8_t MFRFwCmdRmng = 0x10;
constexpr uint8_t MFRFwCmdOTPConfSTO = 0x11;
constexpr uint8_t MFRFwCmdOTPFileInvd = 0x12;
constexpr uint8_t MFRFwCmdGetCRC = 0x2D;
constexpr int XDPE15284CConfSize = 1344;
constexpr int XDPE19283BConfSize = 1416;
constexpr uint8_t VRWarnRemaining = 3;
constexpr uint8_t SectTrim = 0x02;

constexpr uint16_t MFRDefaultWaitTime = 20;
constexpr uint16_t MFROTPFileInvalidationWaitTime = 500;
constexpr uint16_t MFRSectionInvalidationWaitTime = 40;

const char* const AddressField = "PMBus Address :";
const char* const ChecksumField = "Checksum :";
const char* const DataStartTag = "Configuration Data]";
const char* const DataEndTag = "[End Configuration Data]";
const char* const DataComment = "//";
const char* const DataXV = "XV";

XDPE1X2XX::XDPE1X2XX(sdbusplus::async::context& ctx, uint16_t bus,
                     uint16_t address) :
    VoltageRegulator(ctx), i2cInterface(phosphor::i2c::I2C(bus, address))
{}

sdbusplus::async::task<bool> XDPE1X2XX::getDeviceId(uint8_t* deviceID)
{
    uint8_t tbuf[16] = {0};
    tbuf[0] = PMBusICDeviceID;
    tbuf[1] = 2;
    uint8_t tSize = 1;
    uint8_t rbuf[16] = {0};
    uint8_t rSize = IFXICDeviceIDLen + 1;

    if (!(co_await this->i2cInterface.sendReceive(tbuf, tSize, rbuf, rSize)))
    {
        error("Failed to get device ID");
        co_return false;
    }

    std::memcpy(deviceID, &rbuf[1], IFXICDeviceIDLen);

    co_return true;
}

sdbusplus::async::task<bool> XDPE1X2XX::mfrFWcmd(uint8_t cmd, uint16_t pcTime,
                                                 uint8_t* data, uint8_t* resp)
{
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
        if (!(co_await this->i2cInterface.sendReceive(tBuf, tSize, rBuf,
                                                      rSize)))
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
    if (!(co_await this->i2cInterface.sendReceive(tBuf, tSize, rBuf, rSize)))
    {
        error("Failed to send MFR command: {CMD}", "CMD",
              std::string("IFXMFRFwCmd"));
        co_return false;
    }

    co_await sdbusplus::async::sleep_for(ctx,
                                         std::chrono::milliseconds(pcTime));

    if (resp)
    {
        tBuf[0] = IFXMFRFwCmdData;
        tSize = 1;
        rSize = 6;
        if (!(co_await this->i2cInterface.sendReceive(tBuf, tSize, rBuf,
                                                      rSize)))
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

sdbusplus::async::task<bool> XDPE1X2XX::getRemainingWrites(uint8_t* remain)
{
    uint8_t tBuf[16] = {0};
    uint8_t rBuf[16] = {0};
    uint8_t devId[2] = {0};

    if (!(co_await this->mfrFWcmd(MFRFwCmdRmng, MFRDefaultWaitTime, tBuf,
                                  rBuf)))
    {
        error("Failed to request remaining writes");
        co_return false;
    }

    if (!(co_await this->getDeviceId(devId)))
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

sdbusplus::async::task<bool> XDPE1X2XX::getCRC(uint32_t* checksum)
{
    uint8_t tBuf[16] = {0};
    uint8_t rBuf[16] = {0};

    if (!(co_await this->mfrFWcmd(MFRFwCmdGetCRC, MFRDefaultWaitTime, tBuf,
                                  rBuf)))
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

sdbusplus::async::task<bool> XDPE1X2XX::program(bool force)
{
    uint8_t tBuf[16] = {0};
    uint8_t rBuf[16] = {0};
    uint8_t remain = 0;
    uint32_t sum = 0;
    int size = 0;

    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    if (!(co_await getCRC(&sum)))
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
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
    if (!(co_await this->getRemainingWrites(&remain)))
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
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
    // 0xfe 0xfe 0x00 0x00 instructs the command to reprogram all header codes
    // and XVcode. If the old sections are not invalidated in OTP, they can
    // affect the CRC calculation.

    tBuf[0] = 0xfe;
    tBuf[1] = 0xfe;
    tBuf[2] = 0x00;
    tBuf[3] = 0x00;

    if (!(co_await this->mfrFWcmd(MFRFwCmdOTPFileInvd,
                                  MFROTPFileInvalidationWaitTime, tBuf, NULL)))
    {
        error("Failed to program the VR - Invalidation of currect FW");
        co_return false;
    }

    for (int i = 0; i < configuration.sectCnt; i++)
    {
        struct configSect* sect = &configuration.section[i];
        if (sect == NULL)
        {
            error(
                "Failed to program the VR - unexpected NULL section in config");
            co_return false;
        }

        if ((i <= 0) || (sect->type != configuration.section[i - 1].type))
        {
            tBuf[0] = PMBusSTLCml;
            tBuf[1] = 0x1;
            uint8_t tSize = 2;
            uint8_t rSize = 0;
            if (!(co_await this->i2cInterface.sendReceive(tBuf, tSize, rBuf,
                                                          rSize)))
            {
                error("Failed to program the VR on sendReceive {CMD}", "CMD",
                      std::string("PMBusSTLCml"));
                co_return false;
            }

            tBuf[0] = sect->type;
            tBuf[1] = 0x00;
            tBuf[2] = 0x00;
            tBuf[3] = 0x00;

            if (!(co_await this->mfrFWcmd(MFRFwCmdOTPFileInvd,
                                          MFRSectionInvalidationWaitTime, tBuf,
                                          NULL)))
            {
                error("Failed to program VR on mfrFWCmd on {CMD}", "CMD",
                      std::string("MFRFwCmdOTPFileInvd"));
                co_return false;
            }

            tBuf[0] = IFXMFRAHBAddr;
            tBuf[1] = 4;
            tBuf[2] = 0x00;
            tBuf[3] = 0xe0;
            tBuf[4] = 0x05;
            tBuf[5] = 0x20;
            tSize = 6;
            rSize = 0;

            if (!(co_await this->i2cInterface.sendReceive(tBuf, tSize, rBuf,
                                                          rSize)))
            {
                error("Failed to program VR on sendReceive on {CMD}", "CMD",
                      std::string("IFXMFRAHBAddr"));
                co_return false;
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
            if (!(co_await this->i2cInterface.sendReceive(tBuf, tSize, rBuf,
                                                          rSize)))
            {
                error("Failed to program the VR on sendReceive {CMD}", "CMD",
                      std::string("IFXMFRRegWrite"));
                co_return false;
            }
            co_await sdbusplus::async::sleep_for(ctx,
                                                 std::chrono::milliseconds(10));
        }

        size += sect->dataCnt * 4;
        if ((i + 1 >= configuration.sectCnt) ||
            (sect->type != configuration.section[i + 1].type))
        {
            // wait for programming soak (2ms/byte, at least 200ms)
            // ex: Config (604 bytes): (604 / 50) + 2 = 14 (1400 ms)
            uint16_t soakTime = 100 * ((size / 50) + 2);

            // Upload to scratchpad
            debug("Upload from scratch pad to OTP");
            std::memcpy(tBuf, &size, 2);
            tBuf[2] = 0x00;
            tBuf[3] = 0x00;
            if (!(co_await this->mfrFWcmd(MFRFwCmdOTPConfSTO, soakTime, tBuf,
                                          NULL)))
            {
                error("Failed to program the VR on mfrFWcmd {CMD}", "CMD",
                      std::string("MFRFwCmdOTPConfSTO"));
                co_return false;
            }

            tBuf[0] = PMBusSTLCml;
            uint8_t tSize = 1;
            uint8_t rSize = 1;
            if (!(co_await this->i2cInterface.sendReceive(rBuf, tSize, tBuf,
                                                          rSize)))
            {
                error("Failed to program VR on sendReceive {CMD}", "CMD",
                      std::string("PMBusSTLCml"));
                co_return false;
            }
            if (rBuf[0] & 0x01)
            {
                error("Failed to program VR - response code invalid");
                co_return false;
            }
        }
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

bool XDPE1X2XX::checkImage()
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
            return false;
        }

        crc = calcCRC32(&sect->data[2], 2);
        if (crc != sect->data[2])
        {
            error("Failed to check image - first CRC value mismatch");
            return false;
        }
        sum += crc;

        // check CRC of section data
        crc = calcCRC32(&sect->data[3], sect->dataCnt - 4);
        if (crc != sect->data[sect->dataCnt - 1])
        {
            error("Failed to check image - second CRC value mismatch");
            return false;
        }
        sum += crc;
    }

    if (sum != configuration.sumExp)
    {
        error("Failed to check image - third CRC value mismatch");
        return false;
    }

    return true;
}

sdbusplus::async::task<bool> XDPE1X2XX::verifyImage(const uint8_t* image,
                                                    size_t imageSize)
{
    if (parseImage(image, imageSize) < 0)
    {
        error("Failed to update firmware on parsing Image");
        co_return false;
    }

    if (!checkImage())
    {
        error("Failed to update firmware on check image");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> XDPE1X2XX::updateFirmware(bool force)
{
    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    bool ret = co_await program(force);
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    if (!ret)
    {
        error("Failed to update firmware on program");
    }

    // Reset the configuration
    configuration.addr = 0;
    configuration.totalCnt = 0;
    configuration.sumExp = 0;
    configuration.sectCnt = 0;
    for (int i = 0; i <= MaxSectCnt - 1; i++)
    {
        configuration.section[i].type = 0;
        configuration.section[i].dataCnt = 0;
        for (int j = 0; j <= MaxSectDataCnt; j++)
        {
            configuration.section[i].data[j] = 0;
        }
    }

    co_return ret;
}

sdbusplus::async::task<bool> XDPE1X2XX::reset()
{
    if (!(co_await mfrFWcmd(MFRFwCmdReset, VRResetDelay, NULL, NULL)))
    {
        error("Failed to reset the VR");
        co_return false;
    }

    co_return true;
}

uint32_t XDPE1X2XX::calcCRC32(const uint32_t* data, int len)
{
    if (data == NULL)
    {
        return 0;
    }

    uint32_t crc = 0xFFFFFFFF;
    for (int i = 0; i < len; i++)
    {
        crc ^= data[i];

        for (int b = 0; b < 32; b++)
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
