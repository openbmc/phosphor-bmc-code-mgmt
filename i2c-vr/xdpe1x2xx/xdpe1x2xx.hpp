#pragma once

#include "common/include/i2c/i2c.hpp"
#include "i2c-vr/vr.hpp"

#include <sdbusplus/async.hpp>

#include <cstdint>

namespace phosphor::software::VR
{

class XDPE1X2XX : public VoltageRegulator
{
  public:
    XDPE1X2XX(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address);

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;

    sdbusplus::async::task<bool> updateFirmware(bool force) final;
    sdbusplus::async::task<bool> reset() final;

    sdbusplus::async::task<bool> getCRC(uint32_t* checksum) final;
    bool forcedUpdateAllowed() final;

  private:
    static const int MaxSectCnt = 16;
    static const int MaxSectDataCnt = 200;

    struct configSect
    {
        uint8_t type;
        uint16_t dataCnt;
        uint32_t data[MaxSectDataCnt];
    };

    struct xdpe1x2xxConfig
    {
        uint8_t addr;
        uint16_t totalCnt;
        uint32_t sumExp;
        uint8_t sectCnt;
        struct configSect section[MaxSectCnt];
    };

    sdbusplus::async::task<bool> getDeviceId(uint8_t* deviceId);
    sdbusplus::async::task<bool> mfrFWcmd(uint8_t cmd, uint8_t* data,
                                          uint8_t* resp);
    sdbusplus::async::task<bool> getRemainingWrites(uint8_t* remain);
    sdbusplus::async::task<bool> program(bool force);

    int parseImage(const uint8_t* image, size_t imageSize);
    int checkImage();

    static uint32_t calcCRC32(const uint32_t* data, int len);
    static int getConfigSize(uint8_t deviceId, uint8_t revision);
    static int lineSplit(char** dst, char* src, char* delim);

    phosphor::i2c::I2C i2cInterface;

    struct xdpe1x2xxConfig configuration;
};

} // namespace phosphor::software::VR
