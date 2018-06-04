#include "activation.hpp"
#include "config.h"
#include "flash.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

void Flash::write()
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

void Flash::monitorStateChanges(sdbusplus::message::message& msg)
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

    if (newStateUnit == rwServiceFile && newStateResult == "done")
    {
        parent.rwVolumeCreated = true;
        parent.activationProgress->progress(
            parent.activationProgress->progress() + 20);
    }

    if (newStateUnit == roServiceFile && newStateResult == "done")
    {
        parent.roVolumeCreated = true;
        parent.activationProgress->progress(
            parent.activationProgress->progress() + 50);
    }

    if (newStateUnit == ubootVarsServiceFile && newStateResult == "done")
    {
        parent.ubootEnvVarsUpdated = true;
    }

    if (newStateUnit == rwServiceFile || newStateUnit == roServiceFile ||
        newStateUnit == ubootVarsServiceFile)
    {
        if (newStateResult == "failed" || newStateResult == "dependency")
        {
            parent.Activation::activation(
                softwareServer::Activation::Activations::Failed);
        }
        else if ((parent.rwVolumeCreated &&
                  parent.roVolumeCreated) ||   // Volumes were created
                 (parent.ubootEnvVarsUpdated)) // Environment variables were
                                               // updated
        {
            parent.Activation::activation(
                softwareServer::Activation::Activations::Activating);
        }
    }

    return;
}

} // namespace updater
} // namespace software
} // namepsace phosphor
