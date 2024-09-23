#include "software_manager.hpp"

#include "sdbusplus/async/timer.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

SoftwareManager::SoftwareManager(sdbusplus::async::context& io,
                                 const std::string& configType, bool isDryRun,
                                 bool debug) :
    dryRun(isDryRun), debug(debug), configType(configType), io(io),
    bus(io.get_bus()), manager(io, "/")
{
    lg2::debug("initialized SoftwareManager with configuration type {TYPE}",
               "TYPE", configType);

    if (debug)
    {
        lg2::info("[debug] enabled debug mode");
    }
}

std::string SoftwareManager::getConfigurationInterface()
{
    return "xyz.openbmc_project.Configuration." + this->configType;
}

void SoftwareManager::requestBusName()
{
    // design: service name: xyz.openbmc_project.Software.<deviceX>
    const std::string serviceNameFull =
        "xyz.openbmc_project.Software." + this->configType;

    lg2::debug("requesting dbus name {BUSNAME}", "BUSNAME", serviceNameFull);

    bus.request_name(serviceNameFull.c_str());
}

std::string SoftwareManager::getConfigurationInterfaceMuxGpios()
{
    return getConfigurationInterface() + ".MuxGpios";
}

// NOLINTBEGIN
sdbusplus::async::task<bool> SoftwareManager::setHostPowerstate(bool state)
// NOLINTEND
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
        co_await proxy.set_property(io, "RequestedHostTransition", von);
        targetState = "xyz.openbmc_project.State.Host.HostState.Running";
    }
    else
    {
        co_await proxy.set_property(io, "RequestedHostTransition", voff);
        targetState = "xyz.openbmc_project.State.Host.HostState.Off";
    }

    lg2::debug("[PWR] requested host transition to {STATE}", "STATE",
               targetState);

    lg2::debug("[PWR] async sleep to wait for state transition");
    co_await sdbusplus::async::sleep_for(io, std::chrono::seconds(10));

    auto actualOpt = co_await getHostPowerstate();

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

// NOLINTBEGIN
sdbusplus::async::task<std::optional<bool>> SoftwareManager::getHostPowerstate()
// NOLINTEND
{
    auto proxy = sdbusplus::async::proxy()
                     .service("xyz.openbmc_project.State.Host")
                     .path("/xyz/openbmc_project/state/host0")
                     .interface("xyz.openbmc_project.State.Host");

    std::string stateOn = "xyz.openbmc_project.State.Host.HostState.Running";
    std::string stateOff = "xyz.openbmc_project.State.Host.HostState.Off";

    std::string res =
        co_await proxy.get_property<std::string>(io, "CurrentHostState");

    if (res == stateOn)
    {
        co_return true;
    }
    else if (res == stateOff)
    {
        co_return false;
    }

    lg2::error("[PWR] unexpected power state: {STATE}", "STATE", res);

    co_return true;
}
