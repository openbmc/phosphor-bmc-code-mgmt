#include "activation.hpp"
#include "item_updater.hpp"
#include "config.h"
#include "serialize.hpp"
#include <phosphor-logging/log.hpp>

#ifdef WANT_SIGNATURE_VERIFY
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include "image_verify.hpp"
#endif

namespace phosphor
{
namespace software
{
namespace updater
{

namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

using namespace phosphor::logging;

#ifdef WANT_SIGNATURE_VERIFY
using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
namespace control = sdbusplus::xyz::openbmc_project::Control::server;
#endif

void Activation::subscribeToSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                            SYSTEMD_INTERFACE, "Subscribe");
    this->bus.call_noreply(method);

    return;
}

void Activation::unsubscribeFromSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                            SYSTEMD_INTERFACE, "Unsubscribe");
    this->bus.call_noreply(method);

    return;
}

auto Activation::activation(Activations value) -> Activations
{

    if ((value != softwareServer::Activation::Activations::Active) &&
        (value != softwareServer::Activation::Activations::Activating))
    {
        redundancyPriority.reset(nullptr);
    }

    if (value == softwareServer::Activation::Activations::Activating)
    {
        if (rwVolumeCreated == false && roVolumeCreated == false)
        {
            parent.freeSpace();

            if (!activationProgress)
            {
                activationProgress =
                    std::make_unique<ActivationProgress>(bus, path);
            }

            if (!activationBlocksTransition)
            {
                activationBlocksTransition =
                    std::make_unique<ActivationBlocksTransition>(bus, path);
            }

#ifdef WANT_SIGNATURE_VERIFY
            using Signature = phosphor::software::image::Signature;

            fs::path uploadDir(IMG_UPLOAD_DIR);

            Signature signature(uploadDir / versionId, SIGNED_IMAGE_CONF_PATH);

            // Validate the signed image.
            if (!signature.verify())
            {
                log<level::ERR>("Error occurred during image validation");
                report<InternalFailure>();

                // Stop the activation process, if fieldMode is enabled.
                if (parent.control::FieldMode::fieldModeEnabled())
                {
                    // Cleanup
                    activationBlocksTransition.reset(nullptr);
                    activationProgress.reset(nullptr);

                    return softwareServer::Activation::activation(
                        softwareServer::Activation::Activations::Failed);
                }
            }
#endif

            auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                              SYSTEMD_INTERFACE, "StartUnit");
            method.append("obmc-flash-bmc-ubirw.service", "replace");
            bus.call_noreply(method);

            auto roServiceFile =
                "obmc-flash-bmc-ubiro@" + versionId + ".service";
            method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                         SYSTEMD_INTERFACE, "StartUnit");
            method.append(roServiceFile, "replace");
            bus.call_noreply(method);

            activationProgress->progress(10);
        }
        else if (rwVolumeCreated == true && roVolumeCreated == true)
        {
            if (ubootEnvVarsUpdated == false)
            {
                activationProgress->progress(90);

                if (!redundancyPriority)
                {
                    redundancyPriority = std::make_unique<RedundancyPriority>(
                        bus, path, *this, 0);
                }
            }
            else
            {
                activationProgress->progress(100);

                activationBlocksTransition.reset(nullptr);
                activationProgress.reset(nullptr);

                rwVolumeCreated = false;
                roVolumeCreated = false;
                ubootEnvVarsUpdated = false;
                Activation::unsubscribeFromSystemdSignals();

                // Remove version object from image manager
                Activation::deleteImageManagerObject();

                // Create active association
                parent.createActiveAssociation(path);

                return softwareServer::Activation::activation(
                    softwareServer::Activation::Activations::Active);
            }
        }
    }
    else
    {
        activationBlocksTransition.reset(nullptr);
        activationProgress.reset(nullptr);
    }
    return softwareServer::Activation::activation(value);
}

void Activation::deleteImageManagerObject()
{
    // Call the Delete object for <versionID> inside image_manager
    auto method = this->bus.new_method_call(VERSION_BUSNAME, path.c_str(),
                                            "xyz.openbmc_project.Object.Delete",
                                            "Delete");
    auto mapperResponseMsg = bus.call(method);
    // Check that the bus call didn't result in an error
    if (mapperResponseMsg.is_method_error())
    {
        log<level::ERR>("Error in Deleting image from image manager",
                        entry("VERSIONPATH=%s", path.c_str()));
        return;
    }
}

auto Activation::requestedActivation(RequestedActivations value)
    -> RequestedActivations
{
    rwVolumeCreated = false;
    roVolumeCreated = false;
    ubootEnvVarsUpdated = false;

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
    // Set the priority value so that the freePriority() function can order
    // the versions by priority.
    auto newPriority = softwareServer::RedundancyPriority::priority(value);
    storeToFile(parent.versionId, value);
    parent.parent.freePriority(value, parent.versionId);
    return newPriority;
}

uint8_t RedundancyPriority::sdbusPriority(uint8_t value)
{
    storeToFile(parent.versionId, value);
    return softwareServer::RedundancyPriority::priority(value);
}

void Activation::unitStateChange(sdbusplus::message::message& msg)
{
    if (softwareServer::Activation::activation() !=
        softwareServer::Activation::Activations::Activating)
    {
        return;
    }

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

    return;
}

void ActivationBlocksTransition::enableRebootGuard()
{
    log<level::INFO>("BMC image activating - BMC reboots are disabled.");

    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("reboot-guard-enable.service", "replace");
    bus.call_noreply(method);
}

void ActivationBlocksTransition::disableRebootGuard()
{
    log<level::INFO>("BMC activation has ended - BMC reboots are re-enabled.");

    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("reboot-guard-disable.service", "replace");
    bus.call_noreply(method);
}

} // namespace updater
} // namespace software
} // namespace phosphor
