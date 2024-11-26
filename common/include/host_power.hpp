#pragma once

#include <sdbusplus/async/context.hpp>

namespace phosphor::software::host_power
{

enum HOST_POWER_STATE
{
    POWER_STATE_INVALID,
    POWER_STATE_OFF,
    POWER_STATE_ON,
};

class HostPower
{
  public:
    // @param state   desired powerstate (true means on)
    // @returns       true on success
    static sdbusplus::async::task<bool> setHostPowerstate(
        sdbusplus::async::context& ctx, enum HOST_POWER_STATE state);

    // @returns       true when powered
    // @returns       std::nullopt on failure to query power state
    static sdbusplus::async::task<enum HOST_POWER_STATE> getHostPowerstate(
        sdbusplus::async::context& ctx);
};

}; // namespace phosphor::software::host_power
