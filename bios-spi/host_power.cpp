#include "host_power.hpp"

#include "sdbusplus/async/timer.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

PHOSPHOR_LOG2_USING;

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
    sdbusplus::async::context& ctx, bool state)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto client = sdbusplus::client::xyz::openbmc_project::state::Host(ctx)
                      .service("xyz.openbmc_project.State.Host")
                      .path("/xyz/openbmc_project/state/host0");

    debug("[PWR] changing host power state to {STATE}", "STATE",
          (state) ? "ON" : "OFF");

    auto targetState = (state) ? stateOn : stateOff;
    auto transition = (state) ? transitionOn : transitionOff;

    co_await client.requested_host_transition(transition);

    debug("[PWR] requested host transition to {STATE}", "STATE", targetState);

    debug("[PWR] async sleep to wait for state transition");

    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(10));

    const auto actualState = co_await client.current_host_state();

    if (actualState == targetState)
    {
        debug("[PWR] successfully achieved state {STATE}", "STATE",
              targetState);
        co_return true;
    }
    else
    {
        debug("[PWR] failed to achieve state {STATE}", "STATE", targetState);
        co_return false;
    }
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<std::optional<bool>> HostPower::getHostPowerstate(
    sdbusplus::async::context& ctx)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto client = sdbusplus::client::xyz::openbmc_project::state::Host(ctx)
                      .service("xyz.openbmc_project.State.Host")
                      .path("/xyz/openbmc_project/state/host0");

    auto res = co_await client.current_host_state();

    if (res == stateOn)
    {
        co_return true;
    }
    else if (res == stateOff)
    {
        co_return false;
    }

    error("[PWR] unexpected power state: {STATE}", "STATE", res);

    co_return std::nullopt;
}
