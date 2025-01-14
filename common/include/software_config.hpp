#pragma once

#include "sdbusplus/async/context.hpp"

#include <cstdint>
#include <string>

namespace phosphor::software::config
{

/* This class represents the device software configuration we get from
 * entity-manager via D-Bus. Each Code Updater can create its own configuration
 * class that inherits from this to store additional properties from their
 * device configuration, like bus/address/...
 */
class SoftwareConfig
{
  public:
    SoftwareConfig(const std::string& objPath, uint32_t vendorIANA,
                   const std::string& compatible, const std::string& configType,
                   const std::string& name);

    // The dbus object path this configuration was fetched from
    const std::string objectPath;

    // https://github.com/openbmc/entity-manager/blob/master/schemas/firmware.json

    // 'VendorIANA' field from the EM config
    const uint32_t vendorIANA; // e.g. "0x0000A015", 4 bytes as per PLDM spec

    // 'CompatibleHardware' field from the EM config
    const std::string
        compatibleHardware; // e.g.
                            // "com.meta.Hardware.Yosemite4.MedusaBoard.CPLD.LCMX02_2000HC"

    // 'Name' field from the EM config
    const std::string configName;

    // 'Type' field from the EM config
    const std::string configType;

    // @returns        the object path of the inventory item which
    //                 can be associated with this device.
    sdbusplus::async::task<std::string> getInventoryItemObjectPath(
        sdbusplus::async::context& ctx);
};

}; // namespace phosphor::software::config
