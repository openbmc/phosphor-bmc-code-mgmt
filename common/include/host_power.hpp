#pragma once

#include <sdbusplus/async/context.hpp>
#include <sdbusplus/async/match.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

namespace phosphor::software::host_power
{

const auto stateOn =
    sdbusplus::client::xyz::openbmc_project::state::Host<>::HostState::Running;
const auto stateOff =
    sdbusplus::client::xyz::openbmc_project::state::Host<>::HostState::Off;

using HostState =
    sdbusplus::client::xyz::openbmc_project::state::Host<>::HostState;

class HostPower
{
  public:
    HostPower(sdbusplus::async::context& ctx);

    // @param state   desired powerstate
    // @returns       true on success
    static sdbusplus::async::task<bool> setState(sdbusplus::async::context& ctx,
                                                 HostState state);

    // @returns       host powerstate
    static sdbusplus::async::task<HostState> getState(
        sdbusplus::async::context& ctx);

    sdbusplus::async::match stateChangedMatch;
};

}; // namespace phosphor::software::host_power
