#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <sdbusplus/async.hpp>

namespace SDBusAsync = sdbusplus::async;

namespace phosphor::software::VR
{

enum class VRType
{
    XDPE1X2XX,
    DummyVR,
};

class VoltageRegulator
{
  public:
    explicit VoltageRegulator(SDBusAsync::context& ctx) : ctx(ctx){}
    virtual ~VoltageRegulator() = default;

    // @brief Parses the firmware image into the configuration structure
    //        and verifies its correctness.
    // @return sdbusplus::async::task<bool> true indicates success.
    virtual SDBusAsync::task<bool> verifyImage(const uint8_t* image, size_t imageSize) = 0;

    // @brief Applies update to the voltage regulator
    // @return sdbusplus::async::task<bool> true indicates success.
    virtual SDBusAsync::task<bool> updateFirmware(bool force) = 0;

    // @brief resets the voltage regulator for the update to take effect.
    // @return sdbusplus::async::task<bool> true indicates success.
    virtual SDBusAsync::task<bool> reset() = 0;

    // @brief Requests the CRC value of the voltage regulator over I2C.
    // @param pointer to write the result to.
    // @returns < 0 on error
    virtual bool getCRC(uint32_t* checksum) = 0;

  protected:
    SDBusAsync::context& ctx;
};

std::unique_ptr<VoltageRegulator> create(SDBusAsync::context& ctx, enum VRType vrType, uint8_t bus,
                                         uint8_t address);

bool stringToEnum(std::string& vrStr, VRType& vrType);

}
