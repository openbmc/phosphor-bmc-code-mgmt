#include "activation.hpp"
#include "item_updater.hpp"
#include "config.h"
#include "serialize.hpp"
#include <phosphor-logging/log.hpp>


namespace phosphor
{
namespace software
{
namespace updater
{

namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

using namespace phosphor::logging;

void Activation::subscribeToSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_BUSNAME,
                                            SYSTEMD_PATH,
                                            SYSTEMD_INTERFACE,
                                            "Subscribe");
    this->bus.call_noreply(method);

    return;
}

void Activation::unsubscribeFromSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_BUSNAME,
                                            SYSTEMD_PATH,
                                            SYSTEMD_INTERFACE,
                                            "Unsubscribe");
    this->bus.call_noreply(method);

    return;
}

void Delete::delete_()
{
    parent.parent.erase(parent.versionId);
}

auto Activation::activation(Activations value) ->
        Activations
{

    if (value != softwareServer::Activation::Activations::Active)
    {
        redundancyPriority.reset(nullptr);
    }

    if (value == softwareServer::Activation::Activations::Activating)
    {
        if (rwVolumeCreated == false && roVolumeCreated == false)
        {
            if (!activationProgress)
            {
                activationProgress = std::make_unique<ActivationProgress>(bus,
                                                                          path);
            }

            if (!activationBlocksTransition)
            {
                activationBlocksTransition =
                          std::make_unique<ActivationBlocksTransition>(
                                    bus,
                                    path);
            }

            auto method = bus.new_method_call(
                    SYSTEMD_BUSNAME,
                    SYSTEMD_PATH,
                    SYSTEMD_INTERFACE,
                    "StartUnit");
            method.append("obmc-flash-bmc-ubirw.service", "replace");
            bus.call_noreply(method);

            auto roServiceFile = "obmc-flash-bmc-ubiro@" + versionId +
                    ".service";
            method = bus.new_method_call(
                    SYSTEMD_BUSNAME,
                    SYSTEMD_PATH,
                    SYSTEMD_INTERFACE,
                    "StartUnit");
            method.append(roServiceFile, "replace");
            bus.call_noreply(method);

            activationProgress->progress(10);
        }
        else if (rwVolumeCreated == true && roVolumeCreated == true)
        {
            activationProgress->progress(90);

            if (!redundancyPriority)
            {
                redundancyPriority =
                          std::make_unique<RedundancyPriority>(
                                    bus,
                                    path,
                                    *this,
                                    0);
            }

            activationProgress->progress(100);

            activationBlocksTransition.reset(nullptr);
            activationProgress.reset(nullptr);

            rwVolumeCreated = false;
            roVolumeCreated = false;
            Activation::unsubscribeFromSystemdSignals();

            // Create active association
            parent.createActiveAssociation(path);

            return softwareServer::Activation::activation(
                    softwareServer::Activation::Activations::Active);
        }
    }
    else
    {
        activationBlocksTransition.reset(nullptr);
        activationProgress.reset(nullptr);
    }
    return softwareServer::Activation::activation(value);
}

auto Activation::requestedActivation(RequestedActivations value) ->
        RequestedActivations
{
    rwVolumeCreated = false;
    roVolumeCreated = false;

    if ((value == softwareServer::Activation::RequestedActivations::Active) &&
        (softwareServer::Activation::requestedActivation() !=
                  softwareServer::Activation::RequestedActivations::Active))
    {
        if ((softwareServer::Activation::activation() ==
                    softwareServer::Activation::Activations::Ready) ||
            (softwareServer::Activation::activation() ==
                    softwareServer::Activation::Activations::Failed))
        {
            Activation::activation(
                    softwareServer::Activation::Activations::Activating);

        }
    }
    return softwareServer::Activation::requestedActivation(value);
}

uint8_t RedundancyPriority::priority(uint8_t value)
{
    parent.parent.freePriority(value, parent.versionId);
    storeToFile(parent.versionId, value);

    // Update U-Boot env variable to point to this version if it has the
    // lowest priority. Otherwise, reset the UbootEnvVars to find the lowest
    // priority version and set that in U-Boot.
    if (parent.parent.isLowestPriority(value))
    {
        parent.updateUbootEnvVars();
    }
    else
    {
        parent.parent.resetUbootEnvVars();
    }

    return softwareServer::RedundancyPriority::priority(value);
}

// TODO: openbmc/openbmc#2369 Add recovery policy to updateubootvars
//       unit template.
void Activation::updateUbootEnvVars()
{
    auto method = bus.new_method_call(
            SYSTEMD_BUSNAME,
            SYSTEMD_PATH,
            SYSTEMD_INTERFACE,
            "StartUnit");
    auto updateEnvVarsFile = "obmc-flash-bmc-updateubootvars@" + versionId +
            ".service";
    method.append(updateEnvVarsFile, "replace");
    auto result = bus.call(method);

    //Check that the bus call didn't result in an error
    if (result.is_method_error())
    {
        log<level::ERR>("Failed to update u-boot env variables",
                        entry("VERSIONID=%s", versionId));
    }
}

void Activation::unitStateChange(sdbusplus::message::message& msg)
{
    if (softwareServer::Activation::activation() !=
                softwareServer::Activation::Activations::Activating)
    {
        return;
    }

    uint32_t newStateID {};
    sdbusplus::message::object_path newStateObjPath;
    std::string newStateUnit{};
    std::string newStateResult{};

    //Read the msg and populate each variable
    msg.read(newStateID, newStateObjPath, newStateUnit, newStateResult);

    auto rwServiceFile = "obmc-flash-bmc-ubirw.service";
    auto roServiceFile = "obmc-flash-bmc-ubiro@" + versionId + ".service";

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

    if (rwVolumeCreated && roVolumeCreated)
    {
        Activation::activation(
                softwareServer::Activation::Activations::Activating);
    }

    if ((newStateUnit == rwServiceFile || newStateUnit == roServiceFile) &&
        (newStateResult == "failed" || newStateResult == "dependency"))
    {
        Activation::activation(softwareServer::Activation::Activations::Failed);
    }

    return;
}


} // namespace updater
} // namespace software
} // namespace phosphor
