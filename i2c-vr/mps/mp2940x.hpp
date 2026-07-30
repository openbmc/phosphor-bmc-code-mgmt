#pragma once

#include "common/include/pmbus.hpp"
#include "mps.hpp"

#include <cstddef>
#include <cstdint>

namespace phosphor::software::VR
{

class MP2940X : public MPSVoltageRegulator
{
  public:
    MP2940X(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address) :
        MPSVoltageRegulator(ctx, bus, address),
        i2cAddress(static_cast<uint8_t>(address))
    {}

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;
    sdbusplus::async::task<bool> updateFirmware(bool force) final;
    sdbusplus::async::task<bool> getCRC(uint32_t* checksum) final;
    sdbusplus::async::task<bool> parseDeviceConfiguration() final;
    bool forcedUpdateAllowed() final;

  private:
    static constexpr size_t firstConfigRows = 128;

    uint8_t i2cAddress = 0;
    bool usePEC = false;
    size_t expectedConfigRowCount = 0;

    sdbusplus::async::task<bool> detectPECMode();
    sdbusplus::async::task<bool> programConfigurationData();

    sdbusplus::async::task<bool> setPage(MPSPage page, bool withPEC);
    sdbusplus::async::task<bool> readByte(uint8_t command, uint8_t& value,
                                          bool withPEC);
    sdbusplus::async::task<bool> readWord(uint8_t command, uint16_t& value,
                                          bool withPEC);
};

} // namespace phosphor::software::VR
