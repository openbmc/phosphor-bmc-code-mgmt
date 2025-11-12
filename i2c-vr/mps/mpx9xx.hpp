#pragma once

#include "common/include/pmbus.hpp"
#include "mps.hpp"

namespace phosphor::software::VR
{

enum class MPX9XXCmd : uint8_t;

/**
 * @class MPX9XX
 * @brief Base class for MPX9XX voltage regulators
 *
 * Provides common firmware update steps such as unlocking write protection,
 * programming registers, storing/restoring NVM data, and CRC checks.
 * Derived classes only need to provide the Config ID command.
 */
class MPX9XX : public MPSVoltageRegulator
{
  public:
    MPX9XX(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address) :
        MPSVoltageRegulator(ctx, bus, address)
    {}

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;
    sdbusplus::async::task<bool> updateFirmware(bool force) final;
    sdbusplus::async::task<bool> getCRC(uint32_t* checksum) final;
    sdbusplus::async::task<bool> parseDeviceConfiguration() final;
    bool forcedUpdateAllowed() final;

  private:
    sdbusplus::async::task<bool> checkId(MPX9XXCmd idCmd, uint32_t expected);
    sdbusplus::async::task<bool> unlockWriteProtect();
    sdbusplus::async::task<bool> disableStoreFaultTriggering();
    sdbusplus::async::task<bool> setMultiConfigAddress(uint8_t config);
    sdbusplus::async::task<bool> programConfigData(
        const std::vector<MPSData>& gdata);
    sdbusplus::async::task<bool> programAllRegisters();
    sdbusplus::async::task<bool> storeDataIntoMTP();
    sdbusplus::async::task<bool> restoreDataFromNVM();
    sdbusplus::async::task<bool> checkMTPCRC();

  protected:
    virtual MPX9XXCmd getConfigIdCmd() const = 0;
};

class MP292X : public MPX9XX
{
  public:
    using MPX9XX::MPX9XX;

  protected:
    MPX9XXCmd getConfigIdCmd() const final;
};

class MP994X : public MPX9XX
{
  public:
    using MPX9XX::MPX9XX;

  protected:
    MPX9XXCmd getConfigIdCmd() const final;
};

} // namespace phosphor::software::VR
