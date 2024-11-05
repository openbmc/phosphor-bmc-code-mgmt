#pragma once

#include "fw-update/component/interfaces/i2c/i2c_interface.hpp"

#include <map>

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
    virtual int get_fw_version() = 0;
    int get_fw_full_version();
    int get_fw_active_key();
    int fw_update();
    static enum VRType stringToEnum(std::string& vrType);

  private:
    i2c::I2CInterface* inf;
};

std::unique_ptr<VoltageRegulator> create(std::string& vrType, uint8_t bus,
                                         uint8_t address);

// namespace VR
} // namespace VR
