#pragma once

#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>

#include <optional>

// This function is implemented in src/dbus_helper.cpp in order to avoid linkage
// issues. Explicit instantiation is used, add types when necessary.
template <typename T>
sdbusplus::async::task<std::optional<T>> dbusGetRequiredProperty(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& path, const std::string& intf,
    const std::string& property);
