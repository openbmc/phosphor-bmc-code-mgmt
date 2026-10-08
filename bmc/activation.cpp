#include "activation.hpp"

#include "images.hpp"
#include "item_updater.hpp"
#include "msl_verify.hpp"
#include "serialize.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/exception.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Software/Version/error.hpp>

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

#ifdef WANT_SIGNATURE_VERIFY
#include "image_verify.hpp"
#endif

namespace phosphor
{
namespace software
{
namespace updater
{

namespace softwareServer = sdbusplus::server::xyz::openbmc_project::software;

PHOSPHOR_LOG2_USING;
using namespace phosphor::logging;
using InternalFailure =
    sdbusplus::error::xyz::openbmc_project::common::InternalFailure;

#ifdef WANT_SIGNATURE_VERIFY
namespace control = sdbusplus::server::xyz::openbmc_project::control;
#endif

void Activation::subscribeToSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                            SYSTEMD_INTERFACE, "Subscribe");
    try
    {
        this->bus.call_noreply(method);
    }
    catch (const sdbusplus::exception_t& e)
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
            error("Error subscribing to systemd: {ERROR}", "ERROR", e);
        }
    }

    return;
}

void Activation::unsubscribeFromSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                            SYSTEMD_INTERFACE, "Unsubscribe");
    try
    {
        this->bus.call_noreply(method);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Error unsubscribing from systemd signals: {ERROR}", "ERROR", e);
    }

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
#ifdef WANT_SIGNATURE_VERIFY
        fs::path uploadDir(IMG_UPLOAD_DIR);
        if (!verifySignature(uploadDir / versionId, SIGNED_IMAGE_CONF_PATH))
        {
            using InvalidSignatureErr = sdbusplus::error::xyz::openbmc_project::
                software::version::InvalidSignature;
            report<InvalidSignatureErr>();
            // Stop the activation process, if fieldMode is enabled.
            if (parent.control::FieldMode::fieldModeEnabled())
            {
                return softwareServer::Activation::activation(
                    softwareServer::Activation::Activations::Failed);
            }
        }
#endif

        auto versionStr = parent.versions.find(versionId)->second->version();

        if (!minimum_ship_level::verify(versionStr))
        {
            return softwareServer::Activation::activation(
                softwareServer::Activation::Activations::Failed);
        }

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

#ifdef HOST_BIOS_UPGRADE
        auto purpose = parent.versions.find(versionId)->second->purpose();
        if (purpose == VersionPurpose::Host)
        {
            // Enable systemd signals
            subscribeToSystemdSignals();

            // Set initial progress
            activationProgress->progress(20);

            // Initiate image writing to flash
            flashWriteHost();

            return softwareServer::Activation::activation(value);
        }
#endif

        activationProgress->progress(10);

        parent.freeSpace(*this);

        // Enable systemd signals
        Activation::subscribeToSystemdSignals();

        flashWrite();

#if defined UBIFS_LAYOUT || defined MMC_LAYOUT

        return softwareServer::Activation::activation(value);

#else // STATIC_LAYOUT

        if (parent.runningImageSlot == 0)
        {
            // On primary, update it as before
            onFlashWriteSuccess();
            return softwareServer::Activation::activation(
                softwareServer::Activation::Activations::Active);
        }
        // On secondary, wait for the service to complete
#endif
    }
    else
    {
        activationBlocksTransition.reset(nullptr);
    }
    return softwareServer::Activation::activation(value);
}

void Activation::onFlashWriteSuccess()
{
    activationProgress->progress(100);

    activationBlocksTransition.reset(nullptr);
    activationProgress.reset(nullptr);

    rwVolumeCreated = false;
    roVolumeCreated = false;
    ubootEnvVarsUpdated = false;
    Activation::unsubscribeFromSystemdSignals();

    auto flashId = parent.versions.find(versionId)->second->path();
    storePurpose(flashId, parent.versions.find(versionId)->second->purpose());

    if (!redundancyPriority)
    {
        redundancyPriority =
            std::make_unique<RedundancyPriority>(bus, path, *this, 0);
    }

    if (!parent.useUpdateDBusInterface)
    {
        // Remove version object from image manager
        Activation::deleteImageManagerObject();
    }

    // Create active association
    parent.createActiveAssociation(path);

    // Create updateable association as this
    // can be re-programmed.
    parent.createUpdateableAssociation(path);

    if (Activation::checkApplyTimeImmediate())
    {
        if (otherUpdateInProgress())
        {
            info("Another firmware update is in progress; deferring the BMC "
                 "reboot until it completes.");
            deferRebootUntilOtherUpdatesComplete();
        }
        else
        {
            info("Image Active and ApplyTime is immediate; rebooting BMC.");
            Activation::rebootBmc();
        }
    }
    else
    {
        info("BMC image ready; need reboot to get activated.");
    }

    // Create Update Object for this version.
    parent.createUpdateObject(versionId, path);

    activation(softwareServer::Activation::Activations::Active);
}

void Activation::deleteImageManagerObject()
{
    // Call the Delete object for <versionID> inside image_manager if the object
    // has not already been deleted due to a successful update or Delete call
    const std::string interface = std::string{VERSION_IFACE};
    auto method = this->bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                            MAPPER_BUSNAME, "GetObject");
    method.append(path.c_str());
    method.append(std::vector<std::string>({interface}));

    std::map<std::string, std::vector<std::string>> response;

    try
    {
        auto reply = bus.call(method);
        reply.read(response);
        auto it = response.find(VERSION_IFACE);
        if (it != response.end())
        {
            auto deleteMethod = this->bus.new_method_call(
                VERSION_BUSNAME, path.c_str(),
                "xyz.openbmc_project.Object.Delete", "Delete");
            try
            {
                bus.call_noreply(deleteMethod);
            }
            catch (const sdbusplus::exception_t& e)
            {
                error(
                    "Error deleting image ({PATH}) from image manager: {ERROR}",
                    "PATH", path, "ERROR", e);
                return;
            }
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Error in mapper method call for ({PATH}, {INTERFACE}: {ERROR}",
              "ERROR", e, "PATH", path, "INTERFACE", interface);
    }
    return;
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

void Activation::unitStateChange(sdbusplus::message_t& msg)
{
    if (softwareServer::Activation::activation() !=
        softwareServer::Activation::Activations::Activating)
    {
        return;
    }

#ifdef HOST_BIOS_UPGRADE
    auto purpose = parent.versions.find(versionId)->second->purpose();
    if (purpose == VersionPurpose::Host)
    {
        onStateChangesBios(msg);
        return;
    }
#endif

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
#endif

void ActivationBlocksTransition::enableRebootGuard()
{
    info("BMC image activating - BMC reboots are disabled.");

    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("reboot-guard-enable.service", "replace");
    bus.call_noreply(method);
}

void ActivationBlocksTransition::disableRebootGuard()
{
    info("BMC activation has ended - BMC reboots are re-enabled.");

    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("reboot-guard-disable.service", "replace");
    bus.call_noreply(method);
}

bool Activation::checkApplyTimeImmediate()
{
    if (parent.useUpdateDBusInterface)
    {
        return (applyTime == ApplyTimeIntf::RequestedApplyTimes::Immediate);
    }
    auto service = utils::getService(bus, applyTimeObjPath, applyTimeIntf);
    if (service.empty())
    {
        info("Error getting the service name for BMC image ApplyTime. "
             "The BMC needs to be manually rebooted to complete the image "
             "activation if needed immediately.");
    }
    else
    {
        auto method = bus.new_method_call(service.c_str(), applyTimeObjPath,
                                          dbusPropIntf, "Get");
        method.append(applyTimeIntf, applyTimeProp);

        try
        {
            auto reply = bus.call(method);

            auto result = reply.unpack<std::variant<std::string>>();

            auto applyTime = std::get<std::string>(result);
            if (applyTime == applyTimeImmediate)
            {
                return true;
            }
        }
        catch (const sdbusplus::exception_t& e)
        {
            error("Error in getting ApplyTime: {ERROR}", "ERROR", e);
        }
    }
    return false;
}

#ifdef HOST_BIOS_UPGRADE
void Activation::flashWriteHost()
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    auto biosServiceFile = "obmc-flash-host-bios@" + versionId + ".service";
    method.append(biosServiceFile, "replace");
    try
    {
        auto reply = bus.call(method);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Error in trying to upgrade Host Bios: {ERROR}", "ERROR", e);
        report<InternalFailure>();
    }
}

void Activation::onStateChangesBios(sdbusplus::message_t& msg)
{
    uint32_t newStateID{};
    sdbusplus::object_path newStateObjPath;
    std::string newStateUnit{};
    std::string newStateResult{};

    // Read the msg and populate each variable
    msg.read(newStateID, newStateObjPath, newStateUnit, newStateResult);

    auto biosServiceFile = "obmc-flash-host-bios@" + versionId + ".service";

    if (newStateUnit == biosServiceFile)
    {
        // unsubscribe to systemd signals
        unsubscribeFromSystemdSignals();

        if (newStateResult == "done")
        {
            // Set activation progress to 100
            activationProgress->progress(100);

            // Set Activation value to active
            activation(softwareServer::Activation::Activations::Active);

            info("Bios upgrade completed successfully.");
            parent.biosVersion->version(
                parent.versions.find(versionId)->second->version());

            // Delete the uploaded activation
            ctx.spawn([](auto self) -> sdbusplus::async::task<> {
                self->parent.erase(self->versionId);
                co_return;
            }(this));
        }
        else if (newStateResult == "failed")
        {
            // Set Activation value to Failed
            activation(softwareServer::Activation::Activations::Failed);

            error("Bios upgrade failed.");
        }
    }

    return;
}

#endif

bool Activation::rebootBmc()
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("force-reboot.service", "replace");
    try
    {
        auto reply = bus.call(method);
    }
    catch (const sdbusplus::exception_t& e)
    {
        alert("Error in trying to reboot the BMC. The BMC needs to be manually "
              "rebooted to complete the image activation. {ERROR}",
              "ERROR", e);
        report<InternalFailure>();
        return false;
    }
    return true;
}

std::optional<std::set<std::string>> Activation::activeBlockerPaths()
{
    using ActivationBlocksTransitionIntf = sdbusplus::common::xyz::
        openbmc_project::software::ActivationBlocksTransition;
    using ResourceNotFound =
        sdbusplus::error::xyz::openbmc_project::common::ResourceNotFound;

    auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                      MAPPER_BUSNAME, "GetSubTreePaths");
    method.append(
        "/xyz/openbmc_project/software", 0,
        std::vector<std::string>{ActivationBlocksTransitionIntf::interface});

    std::vector<std::string> paths;
    try
    {
        bus.call(method).read(paths);
    }
    catch (const sdbusplus::exception_t& e)
    {
        // A synchronous bus.call() surfaces a remote D-Bus error as SdBusError
        // carrying the error name, not the typed C++ exception, so match on the
        // name. The mapper returns ResourceNotFound when no object implements
        // the interface - a definitive "no peer update in progress".
        if (e.name() != nullptr &&
            std::string_view(e.name()) == ResourceNotFound::errName)
        {
            return std::set<std::string>{};
        }
        // Any other error means we could not determine the state. Return
        // nullopt ("unknown") so callers defer conservatively rather than
        // reboot into a possibly in-progress update.
        error("Failed to query ActivationBlocksTransition objects: {ERROR}",
              "ERROR", e);
        return std::nullopt;
    }

    // Any such object other than our own is a peer update in progress.
    std::set<std::string> blockers;
    for (const auto& p : paths)
    {
        if (p != path)
        {
            blockers.insert(p);
        }
    }
    return blockers;
}

bool Activation::otherUpdateInProgress()
{
    auto blockers = activeBlockerPaths();
    // Unknown mapper state is treated as "in progress" so we defer rather than
    // risk rebooting into a peer update.
    return !blockers || !blockers->empty();
}

void Activation::deferRebootUntilOtherUpdatesComplete()
{
    // Expose RebootBlocksNewUpdates so other updaters refuse to start new
    // updates while this reboot is pending; otherwise a steady stream of new
    // updates could keep extending the wait up to the timeout.
    rebootBlocksNewUpdates = std::make_unique<RebootBlocksNewUpdatesInherit>(
        bus, path.c_str(),
        RebootBlocksNewUpdatesInherit::action::emit_interface_added);

    // Subscribe before snapshotting so a removal that races the synchronous
    // snapshot below is not lost (sd-bus queues it until the event loop runs).
    // Matches are scoped to the software namespace, and the pending set is
    // driven from the signal payloads rather than re-querying the mapper, which
    // can still report an object whose removal is being handled.
    blockerRemovedMatch = std::make_unique<sdbusplus::bus::match_t>(
        bus,
        sdbusRule::interfacesRemovedAtPath("/xyz/openbmc_project/software/"),
        [this](sdbusplus::message_t& msg) { onBlockerRemoved(msg); });
    blockerAddedMatch = std::make_unique<sdbusplus::bus::match_t>(
        bus, sdbusRule::interfacesAddedAtPath("/xyz/openbmc_project/software/"),
        [this](sdbusplus::message_t& msg) { onBlockerAdded(msg); });

    // Seed the pending set from the mapper snapshot. A nullopt snapshot means
    // the state is unknown; treat it as "not clear" below rather than rebooting
    // blindly.
    auto snapshot = activeBlockerPaths();
    if (snapshot)
    {
        for (const auto& p : *snapshot)
        {
            pendingBlockers.insert(p);
        }
    }

    // Arm the bounded fallback first so it also retries if the reboot request
    // below fails, and bounds the wait if a peer update hangs. The task holds a
    // copy of the shared alive flag, so it is safe even if this Activation is
    // destroyed while it sleeps.
    rebootDeferralAlive = std::make_shared<bool>(true);
    ctx.spawn(rebootAfterTimeout(rebootDeferralAlive));

    // Only reboot now if the mapper positively confirmed no peer update
    // remains. If the state was unknown, defer and let the watches or the
    // timeout drive the reboot instead of risking an interruption.
    if (snapshot && pendingBlockers.empty())
    {
        info("No other firmware updates remain; issuing the deferred BMC "
             "reboot.");
        issueDeferredReboot();
    }
}

void Activation::onBlockerAdded(sdbusplus::message_t& msg)
{
    using ActivationBlocksTransitionIntf = sdbusplus::common::xyz::
        openbmc_project::software::ActivationBlocksTransition;
    // Only the interface names matter, but sdbusplus still deserializes the
    // property values, so the variant must cover the types software objects
    // carry (including Association.Definitions' a(sss)).
    // ActivationBlocksTransition is a property-less marker that always reads
    // cleanly; an interface carrying an unlisted type throws and is dropped
    // quietly below (it is not an ABT add).
    using BasicVariant = std::variant<
        std::string, std::vector<std::string>,
        std::vector<std::tuple<std::string, std::string, std::string>>, int64_t,
        uint64_t, double, int32_t, uint32_t, int16_t, uint16_t, uint8_t, bool>;
    using InterfacesMap =
        std::map<std::string, std::map<std::string, BasicVariant>>;

    if (rebootIssued)
    {
        return;
    }

    sdbusplus::object_path objPath;
    InterfacesMap interfaces;
    try
    {
        msg.read(objPath, interfaces);
    }
    catch (const sdbusplus::exception_t& e)
    {
        // Not an interface set we can read means it is not an ABT add we care
        // about; stay quiet so unrelated software-object creation is not noisy.
        debug("Ignoring unparsable InterfacesAdded: {ERROR}", "ERROR", e);
        return;
    }

    if (objPath.str == path ||
        !interfaces.contains(ActivationBlocksTransitionIntf::interface))
    {
        return;
    }

    // A peer update started while our reboot is deferred; keep deferring.
    pendingBlockers.insert(objPath.str);
}

void Activation::onBlockerRemoved(sdbusplus::message_t& msg)
{
    using ActivationBlocksTransitionIntf = sdbusplus::common::xyz::
        openbmc_project::software::ActivationBlocksTransition;

    if (rebootIssued)
    {
        return;
    }

    sdbusplus::object_path objPath;
    std::vector<std::string> interfaces;
    try
    {
        msg.read(objPath, interfaces);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Failed to parse InterfacesRemoved: {ERROR}", "ERROR", e);
        return;
    }

    if (std::ranges::find(interfaces,
                          ActivationBlocksTransitionIntf::interface) ==
        interfaces.end())
    {
        return;
    }

    pendingBlockers.erase(objPath.str);

    if (pendingBlockers.empty())
    {
        info("Other firmware updates finished; issuing the deferred BMC "
             "reboot.");
        issueDeferredReboot();
    }
}

void Activation::issueDeferredReboot()
{
    if (rebootIssued)
    {
        return;
    }

    // Request the reboot first and only tear the interlock down once it
    // actually succeeded. If the request fails, leave rebootIssued false and
    // keep the timeout armed, so a later watch callback or the timeout retries
    // instead of ending up activated but not rebooted with the interlock
    // disabled.
    if (!rebootBmc())
    {
        error("Deferred BMC reboot request failed; will retry.");
        return;
    }

    // Exactly one reboot is issued. The watches are deliberately left in place
    // - destroying a match from inside its own callback is unsafe - and are
    // torn down when this Activation is destroyed; rebootIssued makes any
    // further callbacks no-ops. Clearing the alive flag makes a still-sleeping
    // timeout task a no-op too.
    rebootIssued = true;
    rebootBlocksNewUpdates.reset();
    if (rebootDeferralAlive)
    {
        *rebootDeferralAlive = false;
    }
}

sdbusplus::async::task<> Activation::rebootAfterTimeout(
    std::shared_ptr<bool> alive)
{
    // Backstop for a peer update whose service died without removing its
    // ActivationBlocksTransition object. It must comfortably exceed the
    // slowest single in-progress update so it never fires mid-write (which
    // would reboot into an update); raise it for platforms whose updates can
    // run longer.
    constexpr auto maxWait = std::chrono::minutes(30);

    co_await sdbusplus::async::sleep_for(ctx, maxWait);

    // This Activation may have been destroyed while we slept. Only touch it if
    // the shared flag still says it is alive; the short-circuit guarantees we
    // never dereference a member on a freed object.
    if (*alive && !rebootIssued)
    {
        warning("Timed out waiting for other updates to finish; issuing the "
                "BMC reboot.");
        issueDeferredReboot();
    }
    co_return;
}

} // namespace updater
} // namespace software
} // namespace phosphor
