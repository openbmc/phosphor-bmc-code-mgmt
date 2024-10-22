#pragma once

#include "common/include/i2c/i2c.hpp"
#include "i2c-vr/vr.hpp"

#include <cstdint>
#include <sdbusplus/async.hpp>

namespace SDBusAsync = sdbusplus::async;

namespace phosphor::software::VR
{

const int SectMaxCnt = 16;
const int MaxSectDataCnt = 200;

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
    struct configSect section[SectMaxCnt];
};

class XDPE1X2XX : public VoltageRegulator
{
  public:
    XDPE1X2XX(SDBusAsync::context& ctx, std::unique_ptr<phosphor::i2c::I2C> i2cInf);
    static std::unique_ptr<VoltageRegulator>
        create(std::unique_ptr<phosphor::i2c::I2C> inf);

    SDBusAsync::task<bool> verifyImage(const uint8_t* image, size_t imageSize) final;

    SDBusAsync::task<bool> updateFirmware(bool force) final;
    SDBusAsync::task<bool> reset() final;

    bool getCRC(uint32_t* checksum) final;

  private:
    int getDeviceId(uint8_t* deviceId);
    int ctlRegs(bool lock, uint32_t password);
    int mfrFWcmd(uint8_t cmd, uint8_t* data, uint8_t* resp);
    int getRemainingWrites(uint8_t* remain);
    int parseImage(const uint8_t* image, size_t imageSize);
    int checkImage();
    int program(bool force);
    int getConfigSize(uint8_t deviceId, uint8_t revision);
    uint32_t calcCRC32(const uint32_t* data, int len);
    int lineSplit(char** dst, char* src, char* delim);

    std::unique_ptr<phosphor::i2c::I2C> I2cInterface;

    uint8_t Id;
    struct xdpe1x2xxConfig configuration;
    };

}
