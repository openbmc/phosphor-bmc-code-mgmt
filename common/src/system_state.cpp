#include "system_state.hpp"

#include "common_config.h"

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

namespace RulesIntf = sdbusplus::match_rules;

using HostStateIntf =
    sdbusplus::client::xyz::openbmc_project::state::Host<void, void>;

using OsStateIntf =
    sdbusplus::common::xyz::openbmc_project::state::operating_system::Status;

const auto transitionOn =
    sdbusplus::client::xyz::openbmc_project::state::Host<>::Transition::On;
const auto transitionOff =
    sdbusplus::client::xyz::openbmc_project::state::Host<>::Transition::Off;

namespace State = sdbusplus::common::xyz::openbmc_project::state;

namespace phosphor::software::system_state
{

const auto host0ObjectPath = sdbusplus::client::xyz::openbmc_project::state::
                                 Host<>::namespace_path::value +
                             std::string("/host0");

constexpr const char* service = "xyz.openbmc_project.State.Host";

SystemState::SystemState(sdbusplus::async::context& ctx) :
    ctx(ctx),
    hostStateChangedMatch(ctx, RulesIntf::propertiesChanged(
                                   host0ObjectPath, HostStateIntf::interface)),
    osStateChangedMatch(ctx, RulesIntf::propertiesChanged(
                                 host0ObjectPath, OsStateIntf::interface))
{}

sdbusplus::async::task<bool> SystemState::setHostState(
    sdbusplus::async::context& ctx, HostState state)
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

    constexpr size_t transitionTimeout = HOST_STATE_TRANSITION_TIMEOUT;

    for (size_t i = 0; i < transitionTimeout; i++)
    {
        co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(1));

        if ((co_await client.current_host_state()) == state)
        {
            debug("Successfully achieved state {STATE}", "STATE", state);
            co_return true;
        }
    }

    error("Failed to achieve state {STATE} before the timeout of {TIMEOUT}s",
          "STATE", state, "TIMEOUT", transitionTimeout);

    co_return false;
}

sdbusplus::async::task<HostState> SystemState::getHostState(
    sdbusplus::async::context& ctx)
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

sdbusplus::async::task<OsState> SystemState::getOsState(
    sdbusplus::async::context& ctx)
{
    auto client = sdbusplus::client::xyz::openbmc_project::state::
                      operating_system::Status(ctx)
                          .service(service)
                          .path(host0ObjectPath);

    auto res = co_await client.operating_system_state();
    co_return res;
}

sdbusplus::async::task<> SystemState::watchState(
    const std::string& expectedState, const char* propertyName,
    sdbusplus::async::match& match,
    std::function<sdbusplus::async::task<std::string>()> getCurrentState,
    StateCallback callback)
{
    /* Check the current state before subscribing to property
     * changes. This avoids missing an earlier state transition and ensures
     * firmware version retrieval is not skipped.
     */
    auto currentState = co_await getCurrentState();
    if (currentState == expectedState)
    {
        co_await callback();
    }

    while (!ctx.stop_requested())
    {
        auto nextResult = co_await match.next<
            std::string, std::map<std::string, std::variant<std::string>>>();

        const auto& [interfaceName, changedProperties] = nextResult;

        auto it = changedProperties.find(propertyName);
        if (it == changedProperties.end())
        {
            continue;
        }

        const auto& matchState = std::get<std::string>(it->second);
        if (matchState != expectedState)
        {
            continue;
        }

        co_await callback();
    }

    co_return;
}

sdbusplus::async::task<> SystemState::watchHostState(HostState expectedState,
                                                     StateCallback callback)
{
    const auto expected = State::convertForMessage(expectedState);

    auto getCurrentHostState = [this]() -> sdbusplus::async::task<std::string> {
        auto state = co_await getHostState(ctx);

        co_return State::convertForMessage(state);
    };

    co_await watchState(expected, "CurrentHostState", hostStateChangedMatch,
                        std::move(getCurrentHostState), std::move(callback));
}

sdbusplus::async::task<> SystemState::watchOsState(OsState expectedState,
                                                   StateCallback callback)
{
    const auto expected =
        State::operating_system::Status::convertOSStatusToString(expectedState);

    auto getCurrentOsState = [this]() -> sdbusplus::async::task<std::string> {
        auto state = co_await getOsState(ctx);

        co_return State::operating_system::Status::convertOSStatusToString(
            state);
    };

    co_await watchState(expected, "OperatingSystemState", osStateChangedMatch,
                        std::move(getCurrentOsState), std::move(callback));
}

} // namespace phosphor::software::system_state
