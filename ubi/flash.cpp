#include "config.h"

#include "activation.hpp"

#include <phosphor-logging/log.hpp>

namespace phosphor
{
namespace software
{
namespace updater
{

namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;
using namespace phosphor::logging;

void Activation::flashWrite()
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("obmc-flash-bmc-ubirw.service", "replace");
    bus.call_noreply(method);

    auto roServiceFile = "obmc-flash-bmc-ubiro@" + versionId + ".service";
    method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                 SYSTEMD_INTERFACE, "StartUnit");
    method.append(roServiceFile, "replace");
    bus.call_noreply(method);

    return;
}

void Activation::onStateChanges(sdbusplus::message::message& msg)
{
    uint32_t newStateID{};
    sdbusplus::message::object_path newStateObjPath;
    std::string newStateUnit{};
    std::string newStateResult{};

    // Read the msg and populate each variable
    msg.read(newStateID, newStateObjPath, newStateUnit, newStateResult);

    auto rwServiceFile = "obmc-flash-bmc-ubirw.service";
    auto roServiceFile = "obmc-flash-bmc-ubiro@" + versionId + ".service";
    auto ubootVarsServiceFile =
        "obmc-flash-bmc-updateubootvars@" + versionId + ".service";
    auto rebootGuardDisableServiceFile = "reboot-guard-disable.service";

    if (newStateUnit == rwServiceFile && newStateResult == "done")
    {
        rwVolumeCreated = true;
        activationProgress->progress(activationProgress->progress() + 20);
    }

    if (newStateUnit == roServiceFile && newStateResult == "done")
    {
        roVolumeCreated = true;
        activationProgress->progress(activationProgress->progress() + 50);
    }

    if (newStateUnit == ubootVarsServiceFile && newStateResult == "done")
    {
        ubootEnvVarsUpdated = true;
    }

    if (newStateUnit == rwServiceFile || newStateUnit == roServiceFile ||
        newStateUnit == ubootVarsServiceFile)
    {
        if (newStateResult == "failed" || newStateResult == "dependency")
        {
            Activation::activation(
                softwareServer::Activation::Activations::Failed);
        }
        else if ((rwVolumeCreated && roVolumeCreated) || // Volumes were created
                 (ubootEnvVarsUpdated)) // Environment variables were updated
        {
            Activation::activation(
                softwareServer::Activation::Activations::Activating);
        }
    }

    if (newStateUnit == rebootGuardDisableServiceFile &&
        newStateResult == "done")
    {
        // Check the reboot request flag set during activation and reboot bmc
        // if needed
        if (rebootRequested == true)
        {
            Activation::unsubscribeFromSystemdSignals();
            log<level::INFO>("ApplyTime is immediate, rebooting bmc after "
                             "image activation");
            Activation::rebootBmc();
        }
    }

    return;
}

} // namespace updater
} // namespace software
} // namespace phosphor
