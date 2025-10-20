#pragma once

#include <sdbusplus/async.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace phosphor::software::VR
{

enum class VRType
{
    XDPE1X2XX,
    ISL69269,
    MP2X6XX,
    MP297X,
    MP5998,
    MP994X,
    RAA22XGen2,
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
    // @return sdbusplus::async::task<bool> true indicates success.
    virtual sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                                     size_t imageSize) = 0;

    // @brief Applies update to the voltage regulator
    // @return sdbusplus::async::task<bool> true indicates success.
    virtual sdbusplus::async::task<bool> updateFirmware(bool force) = 0;

    // @brief Requests the CRC value of the voltage regulator over I2C.
    // @param pointer to write the result to.
    // @returns < 0 on error
    virtual sdbusplus::async::task<bool> getCRC(uint32_t* checksum) = 0;

    // @brief This function returns true if the voltage regulator supports
    //        force of updates.
    virtual bool forcedUpdateAllowed() = 0;

  protected:
    sdbusplus::async::context& ctx;
};

std::unique_ptr<VoltageRegulator> create(sdbusplus::async::context& ctx,
                                         enum VRType vrType, uint16_t bus,
                                         uint16_t address);

bool stringToEnum(std::string& vrStr, VRType& vrType);

} // namespace phosphor::software::VR
