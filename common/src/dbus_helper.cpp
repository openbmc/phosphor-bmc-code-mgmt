#include "dbus_helper.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/message/native_types.hpp>

#include <optional>

PHOSPHOR_LOG2_USING;

template <typename T>
sdbusplus::async::task<std::optional<T>> dbusGetOptionalProperty(
    sdbusplus::async::context& ctx, std::string service,
    sdbusplus::object_path path, std::string intf, std::string property)
{
    const std::string pathStr = path.string();

    auto client =
        sdbusplus::async::proxy().service(service).path(pathStr).interface(
            "org.freedesktop.DBus.Properties");

    try
    {
        std::variant<T> result =
            co_await client.call<std::variant<T>>(ctx, "Get", intf, property);

        co_return std::get<T>(result);
    }
    catch (std::exception& e)
    {
        warning("{MSG}", "MSG", e);
        co_return std::nullopt;
    }
}

template <typename T>
sdbusplus::async::task<std::optional<T>> dbusGetRequiredProperty(
    sdbusplus::async::context& ctx, std::string service,
    sdbusplus::object_path path, std::string intf, std::string property)
{
    std::optional<T> opt =
        co_await dbusGetOptionalProperty<T>(ctx, service, path, intf, property);

    if (!opt.has_value())
    {
        error("Missing property {PROPERTY} on path {PATH}, interface {INTF}",
              "PROPERTY", property, "PATH", path, "INTF", intf);
    }

    co_return opt;
}

template sdbusplus::async::task<std::optional<std::string>>
    dbusGetOptionalProperty<std::string>(
        sdbusplus::async::context& ctx, std::string service,
        sdbusplus::object_path path, std::string intf, std::string property);

template sdbusplus::async::task<std::optional<std::string>>
    dbusGetRequiredProperty<std::string>(
        sdbusplus::async::context& ctx, std::string service,
        sdbusplus::object_path path, std::string intf, std::string property);

template sdbusplus::async::task<std::optional<bool>>
    dbusGetOptionalProperty<bool>(
        sdbusplus::async::context& ctx, std::string service,
        sdbusplus::object_path path, std::string intf, std::string property);

template sdbusplus::async::task<std::optional<bool>>
    dbusGetRequiredProperty<bool>(
        sdbusplus::async::context& ctx, std::string service,
        sdbusplus::object_path path, std::string intf, std::string property);

template sdbusplus::async::task<std::optional<uint64_t>>
    dbusGetOptionalProperty<uint64_t>(
        sdbusplus::async::context& ctx, std::string service,
        sdbusplus::object_path path, std::string intf, std::string property);

template sdbusplus::async::task<std::optional<uint64_t>>
    dbusGetRequiredProperty<uint64_t>(
        sdbusplus::async::context& ctx, std::string service,
        sdbusplus::object_path path, std::string intf, std::string property);

template sdbusplus::async::task<std::optional<uint8_t>>
    dbusGetOptionalProperty<uint8_t>(
        sdbusplus::async::context& ctx, std::string service,
        sdbusplus::object_path path, std::string intf, std::string property);

template sdbusplus::async::task<std::optional<uint8_t>>
    dbusGetRequiredProperty<uint8_t>(
        sdbusplus::async::context& ctx, std::string service,
        sdbusplus::object_path path, std::string intf, std::string property);
