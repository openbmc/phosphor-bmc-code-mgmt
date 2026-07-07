#pragma once

#include "sdbusplus/async/context.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

using DbusVariant =
    std::variant<std::vector<std::string>, std::string, int64_t, uint64_t,
                 double, int32_t, uint32_t, int16_t, uint16_t, uint8_t, bool>;
using DbusPropertyMap = std::map<std::string, DbusVariant>;
using InterfacesMap = std::map<std::string, DbusPropertyMap>;

namespace phosphor::software::device
{
class Device;
}

using namespace phosphor::software::device;

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
    SoftwareConfig(const sdbusplus::object_path& objPath, uint32_t vendorIANA,
                   const std::string& compatible, const std::string& configType,
                   const std::string& name, const std::string& baseInterface,
                   InterfacesMap configInterfaces);

    // The dbus object path this configuration was fetched from
    const sdbusplus::object_path objectPath;

    // https://github.com/openbmc/entity-manager/blob/master/schemas/firmware.json

    // 'Name' field from the EM config
    const std::string configName;

    // 'Type' field from the EM config
    const std::string configType;

    // The EM config interface (e.g., xyz.openbmc_project.Configuration.MP29612)
    const std::string baseInterface;

    template <typename T>
    std::optional<T> getProperty(const std::string& intf,
                                 const std::string& key) const
    {
        if (auto it = configInterfaces.find(intf); it != configInterfaces.end())
        {
            if (auto propIt = it->second.find(key); propIt != it->second.end())
            {
                if (const auto* v = std::get_if<T>(&propIt->second))
                {
                    return *v;
                }
            }
        }
        return std::nullopt;
    }

    template <typename T>
    std::optional<T> getProperty(const std::string& key) const
    {
        return getProperty<T>(baseInterface, key);
    }

    // @returns        the object path of the inventory item which
    //                 can be associated with this device.
    sdbusplus::async::task<std::optional<sdbusplus::object_path>>
        getInventoryItemObjectPath(sdbusplus::async::context& ctx);

  private:
    InterfacesMap configInterfaces;

    // 'VendorIANA' field from the EM config
    const uint32_t vendorIANA; // e.g. "0x0000A015", 4 bytes as per PLDM spec

    // 'CompatibleHardware' field from the EM config
    const std::string
        compatibleHardware; // e.g.
                            // "com.meta.Hardware.Yosemite4.MedusaBoard.CPLD.LCMX02_2000HC"

    friend Device;
};

}; // namespace phosphor::software::config
