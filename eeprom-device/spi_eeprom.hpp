#pragma once

#include "common/include/spi/spi_device.hpp"
#include "eeprom_device_version.hpp"

class SPIEEPROM : public SPIDevice
{
  public:
    SPIEEPROM(sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
              uint64_t spiDeviceIndex,
              const std::vector<std::string>& gpioLinesIn,
              const std::vector<bool>& gpioValuesIn,
              std::unique_ptr<DeviceVersion> deviceVersion,
              SoftwareConfig& config, SoftwareManager* parent) :
        SPIDevice(ctx, spiControllerIndex, spiDeviceIndex, false, gpioLinesIn,
                  gpioValuesIn, config, parent, flashLayoutFlat,
                  flashToolFlashcp),
        deviceVersion(std::move(deviceVersion))
    {}

  private:
    std::string getVersion() const final
    {
        return deviceVersion->getVersion();
    }

    std::unique_ptr<DeviceVersion> deviceVersion;
};
