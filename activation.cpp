#include "activation.hpp"

#include "images.hpp"
#include "item_updater.hpp"
#include "serialize.hpp"

#include <phosphor-logging/log.hpp>
#include <sdbusplus/exception.hpp>

#ifdef WANT_SIGNATURE_VERIFY
#include "image_verify.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#endif

namespace phosphor
{
namespace software
{
namespace updater
{

namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

using namespace phosphor::logging;
using sdbusplus::exception::SdBusError;

#ifdef WANT_SIGNATURE_VERIFY
using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
namespace control = sdbusplus::xyz::openbmc_project::Control::server;
#endif

void Activation::subscribeToSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                            SYSTEMD_INTERFACE, "Subscribe");
    try
    {
        this->bus.call_noreply(method);
    }
    catch (const SdBusError& e)
    {
        if (e.name() != nullptr &&
            strcmp("org.freedesktop.systemd1.AlreadySubscribed", e.name()) == 0)
        {
            // If an Activation attempt fails, the Unsubscribe method is not
            // called. This may lead to an AlreadySubscribed error if the
            // Activation is re-attempted.
        }
        else
        {
            log<level::ERR>("Error subscribing to systemd",
                            entry("ERROR=%s", e.what()));
        }
    }

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
#ifdef UBIFS_LAYOUT
        if (rwVolumeCreated == false && roVolumeCreated == false)
        {
            // Enable systemd signals
            Activation::subscribeToSystemdSignals();

            parent.freeSpace(*this);

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
            fs::path uploadDir(IMG_UPLOAD_DIR);
            if (!verifySignature(uploadDir / versionId, SIGNED_IMAGE_CONF_PATH))
            {
                onVerifyFailed();
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

            flashWrite();

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
#else // !UBIFS_LAYOUT

#ifdef WANT_SIGNATURE_VERIFY
        fs::path uploadDir(IMG_UPLOAD_DIR);
        if (!verifySignature(uploadDir / versionId, SIGNED_IMAGE_CONF_PATH))
        {
            onVerifyFailed();
            // Stop the activation process, if fieldMode is enabled.
            if (parent.control::FieldMode::fieldModeEnabled())
            {
                return softwareServer::Activation::activation(
                    softwareServer::Activation::Activations::Failed);
            }
        }
#endif
        parent.freeSpace(*this);

        flashWrite();

        if (!redundancyPriority)
        {
            redundancyPriority =
                std::make_unique<RedundancyPriority>(bus, path, *this, 0);
        }

        // Remove version object from image manager
        Activation::deleteImageManagerObject();

        // Create active association
        parent.createActiveAssociation(path);

        log<level::INFO>("BMC image ready, need reboot to get activated.");
        return softwareServer::Activation::activation(
            softwareServer::Activation::Activations::Active);
#endif
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
    try
    {
        bus.call_noreply(method);
    }
    catch (const SdBusError& e)
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
    parent.parent.savePriority(parent.versionId, value);
    parent.parent.freePriority(value, parent.versionId);
    return newPriority;
}

uint8_t RedundancyPriority::sdbusPriority(uint8_t value)
{
    parent.parent.savePriority(parent.versionId, value);
    return softwareServer::RedundancyPriority::priority(value);
}

void Activation::unitStateChange(sdbusplus::message::message& msg)
{
    if (softwareServer::Activation::activation() !=
        softwareServer::Activation::Activations::Activating)
    {
        return;
    }

    onStateChanges(msg);

    return;
}

#ifdef WANT_SIGNATURE_VERIFY
bool Activation::verifySignature(const fs::path& imageDir,
                                 const fs::path& confDir)
{
    using Signature = phosphor::software::image::Signature;

    Signature signature(imageDir, confDir);

    return signature.verify();
}

void Activation::onVerifyFailed()
{
    log<level::ERR>("Error occurred during image validation");
    report<InternalFailure>();
}
#endif

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
