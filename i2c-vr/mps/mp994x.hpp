#pragma once

#include "common/include/pmbus.hpp"
#include "mps.hpp"

namespace phosphor::software::VR
{

enum class MP994XCmd : uint8_t;

class MP994X : public MPSVoltageRegulator
{
  public:
    MP994X(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address) :
        MPSVoltageRegulator(ctx, bus, address)
    {}

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;
    sdbusplus::async::task<bool> updateFirmware(bool force) final;
    sdbusplus::async::task<bool> getCRC(uint32_t* checksum) final;
    sdbusplus::async::task<bool> parseDeviceConfiguration() final;
    bool forcedUpdateAllowed() final;

  private:
    sdbusplus::async::task<bool> checkId(MP994XCmd idCmd, uint32_t expected);
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
    virtual MP994XCmd getConfigIdCmd() const;
};

} // namespace phosphor::software::VR
