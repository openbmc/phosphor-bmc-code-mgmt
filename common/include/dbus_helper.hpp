#pragma once

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

PHOSPHOR_LOG2_USING;

// The D-Bus errors that mean the object, interface or property does not
// exist. Any other error (NoReply, Timeout, ...) is transient: Entity-Manager
// can be too busy to answer for a while during its boot-time scans.
inline bool isDbusAbsentError(std::string_view name)
{
    return name == "org.freedesktop.DBus.Error.UnknownInterface" ||
           name == "org.freedesktop.DBus.Error.UnknownProperty" ||
           name == "org.freedesktop.DBus.Error.UnknownObject" ||
           name == "org.freedesktop.DBus.Error.InvalidArgs";
}

// Retries of a transient failure stop once this much time has passed since
// the first attempt (one call can itself wait the 25 s D-Bus timeout).
// Entity-Manager has been seen busy for about 73 s at boot.
constexpr auto dbusTransientRetryWindow = std::chrono::seconds(120);
constexpr auto dbusTransientRetryDelay = std::chrono::seconds(2);

template <typename T>
sdbusplus::async::task<std::optional<T>> dbusGetRequiredProperty(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& path, const std::string& intf,
    const std::string& property)
{
    auto client =
        sdbusplus::async::proxy().service(service).path(path).interface(
            "org.freedesktop.DBus.Properties");

    const auto start = std::chrono::steady_clock::now();
    std::optional<T> opt = std::nullopt;
    std::string lastError;
    bool transient = false;
    int attempt = 1;
    for (;; attempt++)
    {
        transient = false;
        try
        {
            std::variant<T> result = co_await client.call<std::variant<T>>(
                ctx, "Get", intf, property);

            opt = std::get<T>(result);
        }
        catch (const sdbusplus::exception::exception& e)
        {
            lastError = e.what();
            transient = !isDbusAbsentError(e.name());
        }
        catch (const std::exception& e)
        {
            lastError = e.what();
        }
        if (opt || !transient ||
            std::chrono::steady_clock::now() - start >=
                dbusTransientRetryWindow)
        {
            break;
        }
        if (attempt == 1)
        {
            warning("Reading {PROPERTY} on path {PATH}, interface {INTF} "
                    "failed, retrying: {ERROR}",
                    "PROPERTY", property, "PATH", path, "INTF", intf, "ERROR",
                    lastError);
        }
        co_await sdbusplus::async::sleep_for(ctx, dbusTransientRetryDelay);
    }
    if (opt && attempt > 1)
    {
        info("Read {PROPERTY} on path {PATH}, interface {INTF} after "
             "{ATTEMPTS} attempts",
             "PROPERTY", property, "PATH", path, "INTF", intf, "ATTEMPTS",
             attempt);
    }
    if (!opt)
    {
        if (transient)
        {
            error("Property {PROPERTY} on path {PATH}, interface {INTF} "
                  "unreadable after {ATTEMPTS} attempts: {ERROR}",
                  "PROPERTY", property, "PATH", path, "INTF", intf,
                  "ATTEMPTS", attempt, "ERROR", lastError);
        }
        else
        {
            error("Missing property {PROPERTY} on path {PATH}, interface "
                  "{INTF}",
                  "PROPERTY", property, "PATH", path, "INTF", intf);
        }
    }
    co_return opt;
}
