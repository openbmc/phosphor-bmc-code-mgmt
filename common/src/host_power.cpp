#include "host_power.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/async/match.hpp>
#include <sdbusplus/async/proxy.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/message/native_types.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

using sdbusplus::client::xyz::openbmc_project::state::Host;

PHOSPHOR_LOG2_USING;

using namespace std::literals;

namespace RulesIntf = sdbusplus::bus::match::rules;

using StateIntf =
    sdbusplus::client::xyz::openbmc_project::state::Host<void, void>;

const auto transitionOn =
    sdbusplus::client::xyz::openbmc_project::state::Host<>::Transition::On;
const auto transitionOff =
    sdbusplus::client::xyz::openbmc_project::state::Host<>::Transition::Off;

namespace phosphor::software::host_power
{

constexpr const char* host0ObjectPath = "/xyz/openbmc_project/state/host0";
constexpr const char* service = "xyz.openbmc_project.State.Host";

HostPower::HostPower(sdbusplus::async::context& ctx) :
    hostStateChangedMatch(ctx, RulesIntf::propertiesChanged(
                                   host0ObjectPath, StateIntf::interface))
{}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> HostPower::setHostPowerstate(
    sdbusplus::async::context& ctx, HostState state)
// NOLINTEND(readability-static-accessed-through-instance)
{
    if (state != stateOn && state != stateOff)
    {
        error("invalid power state {STATE}", "STATE", state);
        co_return false;
    }

    auto client = sdbusplus::client::xyz::openbmc_project::state::Host(ctx)
                      .service(service)
                      .path(host0ObjectPath);

    co_await client.requested_host_transition(
        (state == stateOn) ? transitionOn : transitionOff);

    debug("requested host transition to {STATE}", "STATE", state);

    for (int i = 0; i < 10; i++)
    {
        co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(3));

        if ((co_await client.current_host_state()) == state)
        {
            debug("successfully achieved state {STATE}", "STATE", state);
            co_return true;
        }
    }

    error("failed to achieve state {STATE}", "STATE", state);

    co_return false;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<HostState> HostPower::getHostPowerstate(
    sdbusplus::async::context& ctx)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto client = sdbusplus::client::xyz::openbmc_project::state::Host(ctx)
                      .service(service)
                      .path(host0ObjectPath);

    auto res = co_await client.current_host_state();

    if (res != stateOn && res != stateOff)
    {
        error("unexpected power state: {STATE}", "STATE", res);
    }

    co_return res;
}

} // namespace phosphor::software::host_power
