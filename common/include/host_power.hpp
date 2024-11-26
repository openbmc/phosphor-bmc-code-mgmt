#pragma once

#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

namespace phosphor::software::host_power
{

const auto stateOn =
    sdbusplus::common::xyz::openbmc_project::state::Host::HostState::Running;
const auto stateOff =
    sdbusplus::common::xyz::openbmc_project::state::Host::HostState::Off;

using HostState =
    sdbusplus::common::xyz::openbmc_project::state::Host::HostState;

class HostPower
{
  public:
    // @param state   desired powerstate (true means on)
    // @returns       true on success
    static sdbusplus::async::task<bool> setHostPowerstate(
        sdbusplus::async::context& ctx, HostState state);

    // @returns       true when powered
    // @returns       std::nullopt on failure to query power state
    static sdbusplus::async::task<HostState> getHostPowerstate(
        sdbusplus::async::context& ctx);
};

}; // namespace phosphor::software::host_power
