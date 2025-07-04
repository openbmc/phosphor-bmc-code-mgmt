#pragma once

#include "common/include/pmbus.hpp"
#include "mps.hpp"

namespace phosphor::software::VR
{

class MP2X6XX : public MPSVoltageRegulator
{
  public:
    MP2X6XX(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address) :
        MPSVoltageRegulator(ctx, bus, address)
    {}

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;
    sdbusplus::async::task<bool> updateFirmware(bool force) final;
    sdbusplus::async::task<bool> reset() final;
    sdbusplus::async::task<bool> getCRC(uint32_t* checksum) final;
    sdbusplus::async::task<bool> parseDeviceConfiguration() final;
    bool forcedUpdateAllowed() final;

  private:
    sdbusplus::async::task<bool> checkId(PMBusCmd pmBusCmd, uint32_t expected);
    sdbusplus::async::task<bool> unlockWriteProtect();
    sdbusplus::async::task<bool> storeUserCode();
    sdbusplus::async::task<bool> checkMTPCRC();
    sdbusplus::async::task<bool> selectConfig(uint8_t config);
    sdbusplus::async::task<bool> configAllRegisters();
    sdbusplus::async::task<bool> programConfigData(
        const std::vector<MPSData>& gdata);
};

} // namespace phosphor::software::VR
