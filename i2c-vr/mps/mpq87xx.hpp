#pragma once

#include "common/include/pmbus.hpp"
#include "mps.hpp"

namespace phosphor::software::VR
{

class MPQ87XX : public MPSVoltageRegulator
{
  public:
    MPQ87XX(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address) :
        MPSVoltageRegulator(ctx, bus, address)
    {}

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;
    sdbusplus::async::task<bool> updateFirmware(bool force) final;
    sdbusplus::async::task<bool> getCRC(uint32_t* checksum) final;
    sdbusplus::async::task<bool> parseDeviceConfiguration() final;
    bool forcedUpdateAllowed() final;

  private:
    sdbusplus::async::task<bool> checkId();
    sdbusplus::async::task<bool> storeMTP();
    sdbusplus::async::task<bool> verifyCRC();
    sdbusplus::async::task<bool> programPageRegisters(
        MPSPage page,
        const std::map<uint8_t, std::vector<MPSData>>& groupedData);
};

} // namespace phosphor::software::VR
