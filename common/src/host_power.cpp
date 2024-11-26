#include "host_power.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

PHOSPHOR_LOG2_USING;

using namespace std::literals;
using namespace phosphor::software::host_power;

const auto stateOn =
    sdbusplus::common::xyz::openbmc_project::state::Host::HostState::Running;
const auto stateOff =
    sdbusplus::common::xyz::openbmc_project::state::Host::HostState::Off;

const auto transitionOn =
    sdbusplus::common::xyz::openbmc_project::state::Host::Transition::On;
const auto transitionOff =
    sdbusplus::common::xyz::openbmc_project::state::Host::Transition::Off;

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> HostPower::setHostPowerstate(
    sdbusplus::async::context& ctx, enum HOST_POWER_STATE state)
// NOLINTEND(readability-static-accessed-through-instance)
{
    if (state == POWER_STATE_INVALID)
    {
        co_return false;
    }

    auto client = sdbusplus::client::xyz::openbmc_project::state::Host(ctx)
                      .service("xyz.openbmc_project.State.Host")
                      .path("/xyz/openbmc_project/state/host0");

    auto targetState = (state == POWER_STATE_ON) ? stateOn : stateOff;

    co_await client.requested_host_transition(
        (state == POWER_STATE_ON) ? transitionOn : transitionOff);

    debug("[PWR] requested host transition to {STATE}", "STATE", targetState);

    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(10));

    const auto actualState = co_await client.current_host_state();

    if (actualState == targetState)
    {
        debug("[PWR] successfully achieved state {STATE}", "STATE",
              targetState);
    }
    else
    {
        debug("[PWR] failed to achieve state {STATE}", "STATE", targetState);
    }

    co_return actualState == targetState;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<enum HOST_POWER_STATE> HostPower::getHostPowerstate(
    sdbusplus::async::context& ctx)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto client = sdbusplus::client::xyz::openbmc_project::state::Host(ctx)
                      .service("xyz.openbmc_project.State.Host")
                      .path("/xyz/openbmc_project/state/host0");

    auto res = co_await client.current_host_state();

    if (res == stateOn)
    {
        co_return POWER_STATE_ON;
    }
    else if (res == stateOff)
    {
        co_return POWER_STATE_OFF;
    }

    error("[PWR] unexpected power state: {STATE}", "STATE", res);

    co_return POWER_STATE_INVALID;
}
