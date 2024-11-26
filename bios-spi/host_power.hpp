#pragma once

#include "common/include/device.hpp"
#include "sdbusplus/async/proxy.hpp"

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/timer.hpp>

#include <string>

class HostPower
{
  public:
    // @param state   desired powerstate (true means on)
    // @returns       true on success
    static sdbusplus::async::task<bool> setHostPowerstate(
        sdbusplus::async::context& ctx, bool state);

    // @returns       true when powered
    // @returns       std::nullopt on failure to query power state
    static sdbusplus::async::task<std::optional<bool>> getHostPowerstate(
        sdbusplus::async::context& ctx);
};
