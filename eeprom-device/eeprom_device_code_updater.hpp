#pragma once

#include "common/include/software_manager.hpp"
#include "eeprom_device.hpp"

const std::string configTypeEEPROMDevice = "EEPROMDeviceFirmware";

namespace phosphor::software::eeprom
{

class EEPROMDeviceSoftwareManager : public SoftwareManager
{
  public:
    EEPROMDeviceSoftwareManager(sdbusplus::async::context& ctx) :
        SoftwareManager(ctx, configTypeEEPROMDevice)
    {}

    sdbusplus::async::task<bool> initDevice(const std::string& service,
                                            const std::string& path,
                                            SoftwareConfig& config) final;

  private:
    sdbusplus::async::task<bool> getDeviceProperties(
        const std::string& service, const std::string& path,
        const std::string& intf, uint8_t& bus, uint8_t& address,
        std::string& chipModel);
};

} // namespace phosphor::software::eeprom
