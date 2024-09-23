#pragma once

#include <cstdint>
#include <string>

/* This class represents the device configuration we get from entity-manager via
 * dbus. Each Code Updater can create its own configuration class that inherits
 * from this to store additional properties from their device configuration,
 * like bus/address/...
 */
class DeviceConfig
{
  public:
    DeviceConfig(uint32_t vendorIANA, const std::string& compatible,
                 const std::string& configType,
                 const std::string& objPathConfig, const std::string& name);

    // https://gerrit.openbmc.org/c/openbmc/docs/+/74653
    // properties from the configuration
    const uint32_t vendorIANA; // "0x0000A015", 4 bytes as per PLDM spec

    const std::string
        compatible; // e.g.
                    // "com.meta.Hardware.Yosemite4.MedusaBoard.CPLD.LCMX02_2000HC"

    // 'Name' field from the EM config
    const std::string name;

    // 'Type' field from the EM config
    const std::string configType;

    // the dbus object path of our configuration
    const std::string objPathConfig;
};
