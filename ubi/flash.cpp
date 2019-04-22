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
    auto guardSvcFile = "reboot-guard-disable.service";

    if (newStateUnit == rwServiceFile && newStateResult == "done")
    {
        log<level::INFO>("Inside first rwServiceFile");
        rwVolumeCreated = true;
        activationProgress->progress(activationProgress->progress() + 20);
    }

    if (newStateUnit == roServiceFile && newStateResult == "done")
    {
        log<level::INFO>("Inside rwServiceFile");
        roVolumeCreated = true;
        activationProgress->progress(activationProgress->progress() + 50);
    }

    if (newStateUnit == ubootVarsServiceFile && newStateResult == "done")
    {
        log<level::INFO>("Inside ubootVarsServiceFile");
        ubootEnvVarsUpdated = true;
    }

    if (newStateUnit == rwServiceFile || newStateUnit == roServiceFile ||
        newStateUnit == ubootVarsServiceFile)
    {
        log<level::INFO>("Inside second rwServiceFile");
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

    if (newStateUnit == guardSvcFile && newStateResult == "done")
    {
        log<level::INFO>("BMC reboot guard job got removed");

        //Unsubscribe from systemd signals
        Activation::unsubscribeFromSystemdSignals();

        // Reboot the BMC if the apply time value is immediate
        if (Activation::checkApplyTimeImmediate() == true)
        {
            log<level::INFO>("Going for BMC reboot as apply time is Immediate");
            Activation::rebootBmc();
        }
    }

    return;
}

} // namespace updater
} // namespace software
} // namespace phosphor
