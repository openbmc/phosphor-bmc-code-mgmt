#pragma once

#include <cstdint>
#include <string>

/* This class represents the device software configuration we get from
 * entity-manager via dbus. Each Code Updater can create its own configuration
 * class that inherits from this to store additional properties from their
 * device configuration, like bus/address/...
 */
class SoftwareConfig
{
  public:
    SoftwareConfig(uint32_t vendorIANA, const std::string& compatible,
                   const std::string& configType, const std::string& name);

    // https://github.com/openbmc/entity-manager/blob/master/schemas/firmware.json
    // properties from the configuration

    const uint32_t vendorIANA; // "0x0000A015", 4 bytes as per PLDM spec

    const std::string
        compatibleHardware; // e.g.
                            // "com.meta.Hardware.Yosemite4.MedusaBoard.CPLD.LCMX02_2000HC"

    // 'Name' field from the EM config
    const std::string configName;

    // 'Type' field from the EM config
    const std::string configType;
};
