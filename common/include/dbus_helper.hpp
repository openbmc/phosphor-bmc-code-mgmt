#pragma once

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/message/native_types.hpp>

#include <optional>

PHOSPHOR_LOG2_USING;

template <typename T>
sdbusplus::async::task<std::optional<T>> dbusGetOptionalProperty(
    sdbusplus::async::context& ctx, std::string service,
    sdbusplus::object_path path, std::string intf, std::string property);

template <typename T>
sdbusplus::async::task<std::optional<T>> dbusGetRequiredProperty(
    sdbusplus::async::context& ctx, std::string service,
    sdbusplus::object_path path, std::string intf, std::string property);
