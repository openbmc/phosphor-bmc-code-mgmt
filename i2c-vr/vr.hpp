#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace phosphor::software::VR
{

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

    // @brief Applies update to the voltage regulator
    // @param image
    // @param image_size
    // @returns < 0 on error
    virtual int fwUpdate(const uint8_t* image, size_t image_size, bool force) = 0;

    // @brief Requests the CRC value of the voltage regulator over I2C.
    // @param pointer to write the result to.
    // @returns < 0 on error
    virtual int getCRC(uint32_t* sum) = 0;
};

std::unique_ptr<VoltageRegulator> create(enum VRType vrType, uint8_t bus,
                                         uint8_t address);

bool stringToEnum(VRType& type, std::string& vrType);

}
