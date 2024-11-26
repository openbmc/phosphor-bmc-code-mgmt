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

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> HostPower::setHostPowerstate(
    sdbusplus::async::context& ctx, bool state)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto proxy = sdbusplus::async::proxy()
                     .service("xyz.openbmc_project.State.Host")
                     .path("/xyz/openbmc_project/state/host0")
                     .interface("xyz.openbmc_project.State.Host");

    lg2::info("[PWR] changing host power state to {STATE}", "STATE",
              (state) ? "ON" : "OFF");

    std::string voff = "xyz.openbmc_project.State.Host.Transition.Off";
    std::string von = "xyz.openbmc_project.State.Host.Transition.On";
    std::string targetState;
    if (state)
    {
        co_await proxy.set_property(ctx, "RequestedHostTransition", von);
        targetState = "xyz.openbmc_project.State.Host.HostState.Running";
    }
    else
    {
        co_await proxy.set_property(ctx, "RequestedHostTransition", voff);
        targetState = "xyz.openbmc_project.State.Host.HostState.Off";
    }

    lg2::debug("[PWR] requested host transition to {STATE}", "STATE",
               targetState);

    lg2::debug("[PWR] async sleep to wait for state transition");
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(10));

    auto actualOpt = co_await HostPower::getHostPowerstate(ctx);

    if (actualOpt == std::nullopt)
    {
        co_return false;
    }

    const bool actual = actualOpt.value();

    if (actual == state)
    {
        lg2::debug("[PWR] successfully achieved state {STATE}", "STATE",
                   targetState);
        co_return true;
    }
    else
    {
        lg2::debug("[PWR] failed to achieve state {STATE}", "STATE",
                   targetState);
        co_return false;
    }
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<std::optional<bool>> HostPower::getHostPowerstate(
    sdbusplus::async::context& ctx)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto proxy = sdbusplus::async::proxy()
                     .service("xyz.openbmc_project.State.Host")
                     .path("/xyz/openbmc_project/state/host0")
                     .interface("xyz.openbmc_project.State.Host");

    std::string stateOn = "xyz.openbmc_project.State.Host.HostState.Running";
    std::string stateOff = "xyz.openbmc_project.State.Host.HostState.Off";

    std::string res =
        co_await proxy.get_property<std::string>(ctx, "CurrentHostState");

    if (res == stateOn)
    {
        co_return true;
    }
    else if (res == stateOff)
    {
        co_return false;
    }

    lg2::error("[PWR] unexpected power state: {STATE}", "STATE", res);

    co_return std::nullopt;
}
