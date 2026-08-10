#pragma once

#include "common/include/spi/spi_device.hpp"
#include "eeprom_device_version.hpp"

class SPIEEPROM : public SPIDevice
{
  public:
    SPIEEPROM(sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
              uint64_t spiDeviceIndex, GPIOGroup&& muxGPIO,
              GPIOGroup&& resetGPIO,
              std::unique_ptr<DeviceVersion> deviceVersion,
              SoftwareConfig& config, SoftwareManager* parent) :
        SPIDevice(ctx, spiControllerIndex, spiDeviceIndex, false,
                  std::move(muxGPIO), config, parent, flashLayoutFlat,
                  flashToolFlashcp, std::move(resetGPIO)),
        deviceVersion(std::move(deviceVersion))
    {}

    sdbusplus::async::task<bool> resetDevice() final;

  private:
    std::string getVersion() const final
    {
        return deviceVersion->getVersion();
    }

    std::unique_ptr<DeviceVersion> deviceVersion;
};
