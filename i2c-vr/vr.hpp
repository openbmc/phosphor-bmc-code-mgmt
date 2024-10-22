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

    virtual int fwUpdate(struct xdpe152xxConfig* cfg, bool force) = 0;
  protected:
    uint32_t calcCRC32(const uint32_t* data, int len, uint32_t poly);
};

std::unique_ptr<VoltageRegulator> create(enum VRType vrType, uint8_t bus,
                                         uint8_t address);

enum VRType stringToEnum(std::string& vrType);
