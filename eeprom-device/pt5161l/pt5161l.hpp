#pragma once

#include "eeprom-device/eeprom_device_version.hpp"

class PT5161LDeviceVersion : public DeviceVersion
{
  public:
    using DeviceVersion::DeviceVersion;
    bool getVersionReady() final;
    std::string getVersion() final;
    std::optional<HostPowerInf::HostState> getHostStateToQueryVersion() final;
};
