#pragma once

#include "gpio_controller.hpp"

#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>

#include <optional>

template <typename T>
sdbusplus::async::task<std::optional<T>> dbusGetRequiredProperty(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& path, const std::string& intf,
    const std::string& property);

sdbusplus::async::task<GPIOGroup> dbusGetGPIOs(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& path, const std::string& configIface,
    const std::string& what);
