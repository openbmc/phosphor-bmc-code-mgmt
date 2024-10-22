#pragma once

#include "interfaces/i2c/i2c.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace VR
{
enum class VRType
{
    XDPE152XX,
    XDPE192XX,
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

    static enum VRType stringToEnum(std::string& vrType);

    virtual int get_device_id(uint8_t* devId) = 0;
    virtual int ctl_regs(bool lock) = 0;
    virtual int mfr_fw_cmd(uint8_t cmd, uint8_t* data, uint8_t* resp) = 0;
    virtual int get_remaining_writes(uint8_t* remain) = 0;

  private:
    std::unique_ptr<i2c::I2C> inf;
};

std::unique_ptr<VoltageRegulator> create(std::string& vrType, uint8_t bus,
                                         uint8_t address);

} // namespace VR
