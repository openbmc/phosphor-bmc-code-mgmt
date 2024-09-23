#include "fw_manager.hpp"

#include "pldm_fw_update_verifier.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

FWManager::FWManager(sdbusplus::async::context& io,
                     const std::string& serviceName, bool isDryRun) :
    dryRun(isDryRun), bus(io.get_bus()), io(io)
{
    const std::string serviceNameFull =
        "xyz.openbmc_project.Software.FWUpdate." + serviceName;

    bus.request_name(serviceNameFull.c_str());
}

std::string FWManager::getObjPath(const std::string& swid)
{
    std::string basepath = "/xyz/openbmc_project/Software/";
    return basepath + swid;
}

std::string FWManager::getRandomSoftwareId()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    unsigned int seed = ts.tv_nsec ^ getpid();
    srandom(seed);
    return std::to_string(random());
}

bool FWManager::setHostPowerstate(bool state)
{
    sdbusplus::async::context asyncCtx;

    auto client =
        sdbusplus::async::client_t<
            sdbusplus::client::xyz::openbmc_project::state::Host>(asyncCtx)
            .service("xyz.openbmc_project.State.Host")
            .path("/xyz/openbmc_project/state/host0");

    auto transitionOn =
        sdbusplus::common::xyz::openbmc_project::state::Host::Transition::On;

    lg2::info("changing host power state to {STATE}", "STATE",
              (state) ? "ON" : "OFF");

    auto response = client.requested_host_transition(transitionOn);
    // TODO: check error here
    // TODO: wait for the power state to be actually achieved

    // sdbusplus::common::xyz::openbmc_project::state::Host::HostState
    // currentState;
    auto res = client.current_host_state();

    return true;
}
