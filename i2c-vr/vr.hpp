#pragma once

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

    virtual int fwUpdate(uint8_t* image, size_t image_size, bool force) = 0;
    virtual int getCRC(uint32_t* sum) = 0;
};

std::unique_ptr<VoltageRegulator> create(enum VRType vrType, uint8_t bus,
                                         uint8_t address);

enum VRType stringToEnum(std::string& vrType);
