#pragma once

#include "common/i2c/i2c.hpp"

#include <cstdint>
#include <memory>
#include <string>

enum class VRType
{
    XDPE1X2XX,
    ISL69260,
};

enum
{
    VR_WARN_REMAIN_WR = 3,
};

class VoltageRegulator
{
  public:
    virtual ~VoltageRegulator() = default;

    virtual int getDeviceId(uint8_t* devId) = 0;
    virtual int ctlRegs(bool lock, uint32_t password) = 0;
    virtual int mfrFWcmd(uint8_t cmd, uint8_t* data, uint8_t* resp) = 0;
    virtual int getRemainingWrites(uint8_t* remain) = 0;
    virtual int getCRC(uint32_t* sum) = 0;

  private:
    std::unique_ptr<phosphor::i2c::I2C> inf;
};

std::unique_ptr<VoltageRegulator> create(enum VRType vrType, uint8_t bus,
                                         uint8_t address);

enum VRType stringToEnum(std::string& vrType);
