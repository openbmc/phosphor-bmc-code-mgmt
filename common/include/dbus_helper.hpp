#pragma once

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>

#include <optional>

PHOSPHOR_LOG2_USING;

// Fetch an optional EM configuration property; absence is not an error
// and is logged at debug level only.
template <typename T>
sdbusplus::async::task<std::optional<T>> dbusGetOptionalProperty(
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
        debug("Optional property {PROPERTY} not set on path {PATH}", "PROPERTY",
              property, "PATH", path);
    }
    co_return opt;
}

template <typename T>
sdbusplus::async::task<std::optional<T>> dbusGetRequiredProperty(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& path, const std::string& intf,
    const std::string& property)
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
