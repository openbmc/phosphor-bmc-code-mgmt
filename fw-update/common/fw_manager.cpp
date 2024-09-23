#include "fw_manager.hpp"

#include "sdbusplus/async/timer.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

FWManager::FWManager(sdbusplus::async::context& io,
                     const std::string& serviceName, bool isDryRun,
                     bool debug) :
    dryRun(isDryRun), debug(debug), serviceName(serviceName), bus(io.get_bus()),
    io(io), manager(io, "/")
{
    if (debug)
    {
        lg2::info("[debug] enabled debug mode");
    }
}

void FWManager::requestBusName()
{
    const std::string serviceNameFull =
        "xyz.openbmc_project.Software.FWUpdate." + serviceName;

    bus.request_name(serviceNameFull.c_str());
}

std::string FWManager::getConfigurationInterfaceMuxGpios()
{
    return getConfigurationInterface() + ".MuxGpios";
}

std::string FWManager::getObjPathFromSwid(const std::string& swid)
{
    std::string basepath = "/xyz/openbmc_project/software/";
    return basepath + swid;
}

std::string FWManager::getRandomSoftwareId()
{
    return std::format("swid_{}", getRandomId());
}

long int FWManager::getRandomId()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    unsigned int seed = ts.tv_nsec ^ getpid();
    srandom(seed);
    return random() % 10000;
}

// NOLINTBEGIN
sdbusplus::async::task<bool> FWManager::setHostPowerstate(bool state)
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
sdbusplus::async::task<std::optional<bool>> FWManager::getHostPowerstate()
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
