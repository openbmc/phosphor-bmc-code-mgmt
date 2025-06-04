#pragma once

#include "i2c.hpp"

#include <sdbusplus/async.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace phosphor::software::VR
{

enum class VRType
{
    XDPE1X2XX,
};

class VoltageRegulator
{
  public:
    explicit VoltageRegulator(sdbusplus::async::context& ctx) : ctx(ctx) {}
    virtual ~VoltageRegulator() = default;

    VoltageRegulator(VoltageRegulator& vr) = delete;
    VoltageRegulator& operator=(VoltageRegulator other) = delete;
    VoltageRegulator(VoltageRegulator&& other) = delete;
    VoltageRegulator& operator=(VoltageRegulator&& other) = delete;

    // @brief Parses the firmware image into the configuration structure
    //        and verifies its correctness.
    // @return true indicates success.
    virtual bool verifyImage(const uint8_t* image, size_t imageSize) = 0;

    // @brief Applies update to the voltage regulator
    // @return sdbusplus::async::task<bool> true indicates success.
    virtual sdbusplus::async::task<bool> updateFirmware(bool force) = 0;

    // @brief resets the voltage regulator for the update to take effect.
    // @return sdbusplus::async::task<bool> true indicates success.
    virtual sdbusplus::async::task<bool> reset() = 0;

    // @brief Requests the CRC value of the voltage regulator over I2C.
    // @param pointer to write the result to.
    // @returns < 0 on error
    virtual sdbusplus::async::task<bool> getCRC(
        std::shared_ptr<phosphor::i2c::I2C> intf, uint32_t* checksum) = 0;

    // @brief This function returns true if the voltage regulator supports
    //        force of updates.
    virtual bool forcedUpdateAllowed() = 0;

    std::shared_ptr<i2c::I2C> i2cInterface;

  protected:
    sdbusplus::async::context& ctx;
};

std::unique_ptr<VoltageRegulator> create(sdbusplus::async::context& ctx,
                                         enum VRType vrType, uint16_t bus,
                                         uint16_t address);

bool stringToEnum(std::string& vrStr, VRType& vrType);

} // namespace phosphor::software::VR
