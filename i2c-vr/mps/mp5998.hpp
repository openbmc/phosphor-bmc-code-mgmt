#pragma once

#include "common/include/pmbus.hpp"
#include "mps.hpp"

namespace phosphor::software::VR
{

class MP5998 : public MPSVoltageRegulator
{
  public:
    MP5998(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address) :
        MPSVoltageRegulator(ctx, bus, address), bus(bus), address(address)
    {}

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;
    sdbusplus::async::task<bool> updateFirmware(bool force) final;
    sdbusplus::async::task<bool> reset() final;
    sdbusplus::async::task<bool> getCRC(uint32_t* checksum) final;
    sdbusplus::async::task<bool> parseDeviceConfiguration() final;
    bool forcedUpdateAllowed() final;
    std::set<RequestedApplyTimes> getAllowedApplyTimes() final
    {
        return {RequestedApplyTimes::OnReset};
    }

  private:
    uint16_t bus;
    uint8_t address;

    sdbusplus::async::task<bool> update();
    sdbusplus::async::task<bool> checkId(uint8_t cmd, uint32_t expected);
    sdbusplus::async::task<bool> unlockPasswordProtection();
    sdbusplus::async::task<bool> unlockWriteProtection();
    sdbusplus::async::task<bool> storeMTP();
    sdbusplus::async::task<bool> waitForMTPComplete();
    sdbusplus::async::task<bool> sendRestoreMTPCommand();
    sdbusplus::async::task<bool> restoreMTPAndVerify();
    sdbusplus::async::task<bool> checkEEPROMFaultAfterRestore();
    sdbusplus::async::task<bool> verifyCRC();
    sdbusplus::async::task<bool> programAllRegisters();
};

} // namespace phosphor::software::VR
