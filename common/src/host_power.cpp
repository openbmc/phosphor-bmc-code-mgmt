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

const auto host0ObjectPath = sdbusplus::client::xyz::openbmc_project::state::
                                 Host<>::namespace_path::value +
                             std::string("/host0");

constexpr const char* service = "xyz.openbmc_project.State.Host";

HostPower::HostPower(sdbusplus::async::context& ctx) :
    stateChangedMatch(ctx, RulesIntf::propertiesChanged(host0ObjectPath,
                                                        StateIntf::interface))
{}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> HostPower::setState(sdbusplus::async::context& ctx,
                                                 HostState state)
// NOLINTEND(readability-static-accessed-through-instance)
{
    if (state != stateOn && state != stateOff)
    {
        error("Invalid power state {STATE}", "STATE", state);
        co_return false;
    }

    auto client = sdbusplus::client::xyz::openbmc_project::state::Host(ctx)
                      .service(service)
                      .path(host0ObjectPath);

    co_await client.requested_host_transition(
        (state == stateOn) ? transitionOn : transitionOff);

    debug("Requested host transition to {STATE}", "STATE", state);

    constexpr size_t retries = 4;
    constexpr size_t retryTimeout = 3;

    for (size_t i = 0; i < retries; i++)
    {
        co_await sdbusplus::async::sleep_for(
            ctx, std::chrono::seconds(retryTimeout));

        if ((co_await client.current_host_state()) == state)
        {
            debug("Successfully achieved state {STATE}", "STATE", state);
            co_return true;
        }
    }

    error("Failed to achieve state {STATE} before the timeout of {TIMEOUT}s",
          "STATE", state, "TIMEOUT", retries * retryTimeout);

    co_return false;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<HostState> HostPower::getState(
    sdbusplus::async::context& ctx)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto client = sdbusplus::client::xyz::openbmc_project::state::Host(ctx)
                      .service(service)
                      .path(host0ObjectPath);

    auto res = co_await client.current_host_state();

    if (res != stateOn && res != stateOff)
    {
        error("Unexpected power state: {STATE}", "STATE", res);
    }

    co_return res;
}

} // namespace phosphor::software::host_power
