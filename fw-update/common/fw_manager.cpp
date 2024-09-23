#include "fw_manager.hpp"

#include "pldm_fw_update_verifier.hpp"
#include "sdbusplus/async/timer.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

FWManager::FWManager(sdbusplus::async::context& io,
                     const std::string& serviceName, bool isDryRun) :
    dryRun(isDryRun), serviceName(serviceName), bus(io.get_bus()), io(io)
{}

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
    /*
    auto client =
        sdbusplus::async::client_t<
            sdbusplus::client::xyz::openbmc_project::state::Host>(io)
            .service("xyz.openbmc_project.State.Host")
            .path("/xyz/openbmc_project/state/host0");
        */
    /*
    auto client =
            sdbusplus::client::xyz::openbmc_project::state::Host(io)
            .service("xyz.openbmc_project.State.Host")
            .path("/xyz/openbmc_project/state/host0");
        */
    auto proxy = sdbusplus::async::proxy()
                     .service("xyz.openbmc_project.State.Host")
                     .path("/xyz/openbmc_project/state/host0")
                     .interface("xyz.openbmc_project.State.Host");

    /*
    auto transitionOn =
        sdbusplus::common::xyz::openbmc_project::state::Host::Transition::On;
    auto transitionOff =
        sdbusplus::common::xyz::openbmc_project::state::Host::Transition::Off;
    */

    lg2::info("changing host power state to {STATE}", "STATE",
              (state) ? "ON" : "OFF");

    std::string voff = "xyz.openbmc_project.State.Host.Transition.Off";
    std::string von = "xyz.openbmc_project.State.Host.Transition.On";
    std::string targetState;
    if (state)
    {
        // co_await client.requested_host_transition(transitionOn);
        co_await proxy.set_property(io, "RequestedHostTransition", von);
        targetState = "xyz.openbmc_project.State.Host.HostState.Running";
    }
    else
    {
        // co_await client.requested_host_transition(transitionOff);
        co_await proxy.set_property(io, "RequestedHostTransition", voff);
        targetState = "xyz.openbmc_project.State.Host.HostState.Off";
    }

    lg2::debug("requested host transition to {STATE}", "STATE", targetState);

    lg2::debug("async sleep to wait for state transition");
    co_await sdbusplus::async::sleep_for(io, std::chrono::seconds(10));

    // sdbusplus::common::xyz::openbmc_project::state::Host::HostState
    // currentState;
    // auto res = co_await client.current_host_state();
    auto res = co_await proxy.get_property<std::string>(io, "CurrentHostState");

    if (res == targetState)
    {
        // if (res ==
        // sdbusplus::common::xyz::openbmc_project::state::Host::HostState::Running)
        // {
        lg2::debug("successfully achieved state {STATE}", "STATE", targetState);
        co_return true;
    }
    else
    {
        lg2::debug("failed to achieve state {STATE}", "STATE", targetState);
        co_return false;
    }
}
