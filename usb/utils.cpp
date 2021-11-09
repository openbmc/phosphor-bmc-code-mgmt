#include "config.h"

#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

namespace phosphor
{
namespace usb
{
namespace utils
{

// Get service name
const std::string DBusHandler::getService(const std::string& path,
                                          const std::string& interface) const
{

    using InterfaceList = std::vector<std::string>;
    std::map<std::string, std::vector<std::string>> mapperResponse;

    auto& bus = DBusHandler::getBus();

    auto mapper = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                      MAPPER_INTERFACE, "GetObject");
    mapper.append(path, InterfaceList({interface}));

    auto mapperResponseMsg = bus.call(mapper);
    mapperResponseMsg.read(mapperResponse);
    if (mapperResponse.empty())
    {
        lg2::error(
            "Failed to read getService mapper response, OBJECT_PATH = {PATH}, INTERFACE = {INTERFACE}",
            "PATH", path, "INTERFACE", interface);
        return "";
    }

    // the value here will be the service name
    return mapperResponse.cbegin()->first;
}

// Get the property name
const PropertyValue
    DBusHandler::getProperty(const std::string& objectPath,
                             const std::string& interface,
                             const std::string& propertyName) const
{
    PropertyValue value{};

    auto& bus = DBusHandler::getBus();
    auto service = getService(objectPath, interface);
    if (service.empty())
    {
        return value;
    }

    auto method = bus.new_method_call(service.c_str(), objectPath.c_str(),
                                      DBUS_PROPERTY_IFACE, "Get");
    method.append(interface, propertyName);

    auto reply = bus.call(method);
    reply.read(value);

    return value;
}

// Set property
void DBusHandler::setProperty(const std::string& objectPath,
                              const std::string& interface,
                              const std::string& propertyName,
                              const PropertyValue& value) const
{
    auto& bus = DBusHandler::getBus();
    auto service = getService(objectPath, interface);
    if (service.empty())
    {
        return;
    }

    auto method = bus.new_method_call(service.c_str(), objectPath.c_str(),
                                      DBUS_PROPERTY_IFACE, "Set");
    method.append(interface.c_str(), propertyName.c_str(), value);

    bus.call_noreply(method);
}

} // namespace utils
} // namespace usb
} // namespace phosphor
