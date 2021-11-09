#pragma once
#include <sdbusplus/server.hpp>

#include <map>
#include <vector>
namespace phosphor
{
namespace usb
{
namespace utils
{
constexpr auto DBUS_PROPERTY_IFACE = "org.freedesktop.DBus.Properties";

using PropertyValue = std::variant<std::string>;

/**
 *  @class DBusHandler
 *
 *  Wrapper class to handle the D-Bus calls
 *
 *  This class contains the APIs to handle the D-Bus calls.
 */
class DBusHandler
{
  public:
    /** @brief Get the bus connection. */
    static auto& getBus()
    {
        static auto bus = sdbusplus::bus::new_default();
        return bus;
    }

    /**
     *  @brief Get service name by the path and interface of the DBus.
     *
     *  @param[in] path      -  D-Bus object path
     *  @param[in] interface -  D-Bus Interface
     *
     *  @return std::string  -  the D-Bus service name
     *
     */
    const std::string getService(const std::string& path,
                                 const std::string& interface) const;

    /** @brief Get property(type: variant)
     *
     *  @param[in] objectPath       -   D-Bus object path
     *  @param[in] interface        -   D-Bus interface
     *  @param[in] propertyName     -   D-Bus property name
     *
     *  @return The value of the property(type: variant)
     *
     *  @throw sdbusplus::exception::exception when it fails
     */
    const PropertyValue getProperty(const std::string& objectPath,
                                    const std::string& interface,
                                    const std::string& propertyName) const;

    /** @brief Set D-Bus property
     *
     *  @param[in] objectPath       -   D-Bus object path
     *  @param[in] interface        -   D-Bus interface
     *  @param[in] propertyName     -   D-Bus property name
     *  @param[in] value            -   The value to be set
     *
     *  @throw sdbusplus::exception::exception when it fails
     */
    void setProperty(const std::string& objectPath,
                     const std::string& interface,
                     const std::string& propertyName,
                     const PropertyValue& value) const;
};

} // namespace utils
} // namespace usb
} // namespace phosphor
