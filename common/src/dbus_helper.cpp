#include "dbus_helper.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

template <typename T>
sdbusplus::async::task<std::optional<T>> dbusGetRequiredProperty(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& path, const std::string& intf,
    const std::string& property)
{
    auto client =
        sdbusplus::async::proxy().service(service).path(path).interface(
            "org.freedesktop.DBus.Properties");

    std::optional<T> opt = std::nullopt;
    try
    {
        std::variant<T> result =
            co_await client.call<std::variant<T>>(ctx, "Get", intf, property);

        opt = std::get<T>(result);
    }
    catch (std::exception& e)
    {
        error("Missing property {PROPERTY} on path {PATH}, interface {INTF}",
              "PROPERTY", property, "PATH", path, "INTF", intf);
    }
    co_return opt;
}

template sdbusplus::async::task<std::optional<uint64_t>>
    dbusGetRequiredProperty<uint64_t>(
        sdbusplus::async::context& ctx, const std::string& service,
        const std::string& path, const std::string& intf,
        const std::string& property);
template sdbusplus::async::task<std::optional<std::string>>
    dbusGetRequiredProperty<std::string>(
        sdbusplus::async::context& ctx, const std::string& service,
        const std::string& path, const std::string& intf,
        const std::string& property);

sdbusplus::async::task<GPIOGroup> dbusGetGPIOs(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& path, const std::string& configIface,
    const std::string& what)
{
    std::vector<std::string> gpioLines;
    std::vector<bool> gpioPolarities;

    for (size_t i = 0; true; i++)
    {
        const std::string iface = configIface + std::to_string(i);

        std::optional<std::string> name =
            co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                          iface, "Name");

        std::optional<std::string> polarity =
            co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                          iface, "Polarity");

        if (!name.has_value() || !polarity.has_value())
        {
            break;
        }

        gpioLines.push_back(name.value());
        gpioPolarities.push_back(polarity.value() == "High");
    }

    for (size_t i = 0; i < gpioLines.size(); i++)
    {
        debug("{WHAT} gpio {NAME} polarity = {VALUE}", "WHAT", what, "NAME",
              gpioLines[i], "VALUE", gpioPolarities[i]);
    }

    co_return GPIOGroup(gpioLines, gpioPolarities);
}
