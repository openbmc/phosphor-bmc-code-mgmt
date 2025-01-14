#pragma once

#include "sdbusplus/async/proxy.hpp"
#include "software_config.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>

#include <optional>
#include <string>

template <typename T>
sdbusplus::async::task<std::optional<T>> dbusGetProperty(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& path, const std::string& intf,
    const std::string& property)
{
    auto client =
        sdbusplus::async::proxy().service(service).path(path).interface(
            "org.freedesktop.DBus.Properties");

    try
    {
        std::variant<T> result =
            co_await client.call<std::variant<T>>(ctx, "Get", intf, property);

        T res = std::get<T>(result);
        co_return res;
    }
    catch (std::exception& e)
    {
        co_return std::nullopt;
    }
}

// this variant also logs the error
template <typename T>
sdbusplus::async::task<std::optional<T>> dbusGetRequiredProperty(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& path, const std::string& intf,
    const std::string& property)
{
    std::optional<T> opt =
        co_await dbusGetProperty<T>(ctx, service, path, intf, property);
    if (!opt.has_value())
    {
        lg2::error(
            "[config] missing property {PROPERTY} on path {PATH}, interface {INTF}",
            "PROPERTY", property, "PATH", path, "INTF", intf);
    }
    co_return opt;
}

template <typename T>
sdbusplus::async::task<std::optional<T>> dbusGetRequiredConfigurationProperty(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& path, const std::string& property,
    SoftwareConfig& config)
{
    std::string configIface =
        "xyz.openbmc_project.Configuration." + config.configType;
    std::optional<T> res = co_await dbusGetRequiredProperty<T>(
        ctx, service, path, configIface, property);
    co_return res;
}
