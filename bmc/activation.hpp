#pragma once

#include "config.h"

#include "flash.hpp"
#include "utils.hpp"
#include "xyz/openbmc_project/Software/ActivationProgress/server.hpp"
#include "xyz/openbmc_project/Software/RedundancyPriority/server.hpp"

#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/ActivationBlocksTransition/server.hpp>
#include <xyz/openbmc_project/Software/ApplyTime/common.hpp>
#include <xyz/openbmc_project/Software/RebootBlocksNewUpdates/server.hpp>

#ifdef WANT_SIGNATURE_VERIFY
#include <filesystem>
#include <memory>
#include <set>
#endif

namespace phosphor
{
namespace software
{
namespace updater
{

#ifdef WANT_SIGNATURE_VERIFY
namespace fs = std::filesystem;
#endif

using AssociationList =
    std::vector<std::tuple<std::string, std::string, std::string>>;
using ActivationInherit = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::software::Activation,
    sdbusplus::server::xyz::openbmc_project::association::Definitions>;
using ActivationBlocksTransitionInherit =
    sdbusplus::server::object_t<sdbusplus::server::xyz::openbmc_project::
                                    software::ActivationBlocksTransition>;
using RedundancyPriorityInherit = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::software::RedundancyPriority>;
using ActivationProgressInherit = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::software::ActivationProgress>;
using RebootBlocksNewUpdatesInherit = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::software::RebootBlocksNewUpdates>;
using ApplyTimeIntf =
    sdbusplus::common::xyz::openbmc_project::software::ApplyTime;

constexpr auto applyTimeImmediate =
    "xyz.openbmc_project.Software.ApplyTime.RequestedApplyTimes.Immediate";
constexpr auto applyTimeIntf = "xyz.openbmc_project.Software.ApplyTime";
constexpr auto dbusPropIntf = "org.freedesktop.DBus.Properties";
constexpr auto applyTimeObjPath = "/xyz/openbmc_project/software/apply_time";
constexpr auto applyTimeProp = "RequestedApplyTime";

namespace sdbusRule = sdbusplus::match_rules;

class ItemUpdater;
class Activation;
class RedundancyPriority;

/** @class RedundancyPriority
 *  @brief OpenBMC RedundancyPriority implementation
 *  @details A concrete implementation for
 *  xyz.openbmc_project.Software.RedundancyPriority DBus API.
 */
class RedundancyPriority : public RedundancyPriorityInherit
{
  public:
    /** @brief Constructs RedundancyPriority.
     *
     *  @param[in] bus    - The Dbus bus object
     *  @param[in] path   - The Dbus object path
     *  @param[in] parent - Parent object.
     *  @param[in] value  - The redundancyPriority value
     *  @param[in] freePriority  - Call freePriorioty, default to true
     */
    RedundancyPriority(sdbusplus::bus_t& bus, const std::string& path,
                       Activation& parent, uint8_t value,
                       bool freePriority = true) :
        RedundancyPriorityInherit(bus, path.c_str(),
                                  action::emit_interface_added),
        parent(parent)
    {
        // Set Property
        if (freePriority)
        {
            priority(value);
        }
        else
        {
            sdbusPriority(value);
        }
    }

    /** @brief Overridden Priority property set function, calls freePriority
     *         to bump the duplicated priority values.
     *
     *  @param[in] value - uint8_t
     *
     *  @return Success or exception thrown
     */
    uint8_t priority(uint8_t value) override;

    /** @brief Non-Overriden Priority property set function
     *
     *  @param[in] value - uint8_t
     *
     *  @return Success or exception thrown
     */
    uint8_t sdbusPriority(uint8_t value);

    /** @brief Priority property get function
     *
     *  @returns uint8_t - The Priority value
     */
    using RedundancyPriorityInherit::priority;

    /** @brief Parent Object. */
    Activation& parent;
};

/** @class ActivationBlocksTransition
 *  @brief OpenBMC ActivationBlocksTransition implementation.
 *  @details A concrete implementation for
 *  xyz.openbmc_project.Software.ActivationBlocksTransition DBus API.
 */
class ActivationBlocksTransition : public ActivationBlocksTransitionInherit
{
  public:
    /** @brief Constructs ActivationBlocksTransition.
     *
     *  @param[in] bus    - The Dbus bus object
     *  @param[in] path   - The Dbus object path
     */
    ActivationBlocksTransition(sdbusplus::bus_t& bus, const std::string& path) :
        ActivationBlocksTransitionInherit(bus, path.c_str(),
                                          action::emit_interface_added),
        bus(bus)
    {
        enableRebootGuard();
    }

    ~ActivationBlocksTransition() override
    {
        disableRebootGuard();
    }

    ActivationBlocksTransition(const ActivationBlocksTransition&) = delete;
    ActivationBlocksTransition& operator=(const ActivationBlocksTransition&) =
        delete;
    ActivationBlocksTransition(ActivationBlocksTransition&&) = delete;
    ActivationBlocksTransition& operator=(ActivationBlocksTransition&&) =
        delete;

  private:
    sdbusplus::bus_t& bus;

    /** @brief Enables a Guard that blocks any BMC reboot commands */
    void enableRebootGuard();

    /** @brief Disables any guard that was blocking the BMC reboot */
    void disableRebootGuard();
};

class ActivationProgress : public ActivationProgressInherit
{
  public:
    /** @brief Constructs ActivationProgress.
     *
     * @param[in] bus    - The Dbus bus object
     * @param[in] path   - The Dbus object path
     */
    ActivationProgress(sdbusplus::bus_t& bus, const std::string& path) :
        ActivationProgressInherit(bus, path.c_str(),
                                  action::emit_interface_added)
    {
        progress(0);
    }
};

/** @class Activation
 *  @brief OpenBMC activation software management implementation.
 *  @details A concrete implementation for
 *  xyz.openbmc_project.Software.Activation DBus API.
 */
class Activation : public ActivationInherit, public Flash
{
  public:
    /** @brief Constructs Activation Software Manager
     *
     * @param[in] bus    - The Dbus bus object
     * @param[in] path   - The Dbus object path
     * @param[in] parent - Parent object.
     * @param[in] versionId  - The software version id
     * @param[in] activationStatus - The status of Activation
     * @param[in] assocs - Association objects
     */
    Activation(sdbusplus::async::context& ctx, const std::string& path,
               ItemUpdater& parent, std::string& versionId,
               sdbusplus::server::xyz::openbmc_project::software::Activation::
                   Activations activationStatus,
               AssociationList& assocs) :
        ActivationInherit(ctx.get_bus(), path.c_str(),
                          ActivationInherit::action::defer_emit),
        ctx(ctx), bus(ctx.get_bus()), path(path), parent(parent),
        versionId(versionId),
        systemdSignals(
            bus,
            sdbusRule::type::signal() + sdbusRule::member("JobRemoved") +
                sdbusRule::path("/org/freedesktop/systemd1") +
                sdbusRule::interface("org.freedesktop.systemd1.Manager"),
            std::bind(std::mem_fn(&Activation::unitStateChange), this,
                      std::placeholders::_1))
    {
        // Set Properties.
        activation(activationStatus);
        associations(assocs);

        // Emit deferred signal.
        emit_object_added();
    }

    ~Activation() override
    {
        // A deferred-reboot timeout task may still be sleeping while holding a
        // copy of the shared alive flag; mark this object gone so it does not
        // resume onto freed memory.
        if (rebootDeferralAlive)
        {
            *rebootDeferralAlive = false;
        }
    }

    Activation(const Activation&) = delete;
    Activation& operator=(const Activation&) = delete;
    Activation(Activation&&) = delete;
    Activation& operator=(Activation&&) = delete;

    /** @brief Overloaded Activation property setter function
     *
     * @param[in] value - One of Activation::Activations
     *
     * @return Success or exception thrown
     */
    Activations activation(Activations value) override;

    /** @brief Activation */
    using ActivationInherit::activation;

    /** @brief Overloaded requestedActivation property setter function
     *
     * @param[in] value - One of Activation::RequestedActivations
     *
     * @return Success or exception thrown
     */
    RequestedActivations requestedActivation(
        RequestedActivations value) override;

    /** @brief Overloaded write flash function */
    void flashWrite() override;

    /**
     * @brief Handle the success of the flashWrite() function
     *
     * @details Perform anything that is necessary to mark the activation
     * successful after the image has been written to flash. Sets the Activation
     * value to Active.
     */
    void onFlashWriteSuccess();

#ifdef HOST_BIOS_UPGRADE
    /* @brief write to Host flash function */
    void flashWriteHost();

    /** @brief Function that acts on Bios upgrade service file state changes */
    void onStateChangesBios(sdbusplus::message_t& /*msg*/);
#endif

    /** @brief Overloaded function that acts on service file state changes */
    void onStateChanges(sdbusplus::message_t& /*msg*/) override;

    /** @brief Check if systemd state change is relevant to this object
     *
     * Instance specific interface to handle the detected systemd state
     * change
     *
     * @param[in]  msg       - Data associated with subscribed signal
     *
     */
    void unitStateChange(sdbusplus::message_t& msg);

    /**
     * @brief subscribe to the systemd signals
     *
     * This object needs to capture when it's systemd targets complete
     * so it can keep it's state updated
     *
     */
    void subscribeToSystemdSignals();

    /**
     * @brief unsubscribe from the systemd signals
     *
     * systemd signals are only of interest during the activation process.
     * Once complete, we want to unsubscribe to avoid unnecessary calls of
     * unitStateChange().
     *
     */
    void unsubscribeFromSystemdSignals();

    /**
     * @brief Deletes the version from Image Manager and the
     *        untar image from image upload dir.
     */
    void deleteImageManagerObject();

    /**
     * @brief Determine the configured image apply time value
     *
     * @return true if the image apply time value is immediate
     **/
    bool checkApplyTimeImmediate();

    /**
     * @brief Reboot the BMC. Called when ApplyTime is immediate.
     *
     * @return none
     **/
    void rebootBmc();

    /**
     * @brief Snapshot the object paths currently exposing
     *        xyz.openbmc_project.Software.ActivationBlocksTransition - the
     *        interface an updater exposes while its update must block state
     *        transitions (including a BMC reboot). Our own object is excluded.
     *
     * @return the set of peer-update object paths in progress right now
     **/
    std::set<std::string> activeBlockerPaths();

    /** @brief True if another firmware update is blocking transitions now. */
    bool otherUpdateInProgress();

    /**
     * @brief Defer the BMC reboot until no other update is in progress.
     *
     * Subscribes to InterfacesAdded/Removed under the software namespace and
     * tracks the set of peer updates exposing ActivationBlocksTransition
     * purely from the signal payloads, rebooting once the set is empty. Bounded
     * by a timeout so a stuck peer update cannot defer the reboot forever.
     **/
    void deferRebootUntilOtherUpdatesComplete();

    /** @brief InterfacesAdded handler: track a peer update that starts while
     *         the reboot is deferred, so the reboot keeps waiting for it. */
    void onBlockerAdded(sdbusplus::message_t& msg);

    /** @brief InterfacesRemoved handler: drop a completed peer update and
     *         issue the reboot once none remain. */
    void onBlockerRemoved(sdbusplus::message_t& msg);

    /** @brief Tear down the watches and marker and issue the deferred reboot,
     *         exactly once. */
    void issueDeferredReboot();

    /** @brief Bounded fallback that reboots even if a peer update never
     *         completes. Holds a shared "alive" flag so it never touches this
     *         Activation after it has been destroyed. */
    sdbusplus::async::task<> rebootAfterTimeout(std::shared_ptr<bool> alive);

    /** @brief D-Bus context */
    sdbusplus::async::context& ctx;

    /** @brief Persistent sdbusplus DBus bus connection */
    sdbusplus::bus_t& bus;

    /** @brief Persistent DBus object path */
    std::string path;

    /** @brief Parent Object. */
    ItemUpdater& parent;

    /** @brief Version id */
    std::string versionId;

    /** @brief Persistent ActivationBlocksTransition dbus object */
    std::unique_ptr<ActivationBlocksTransition> activationBlocksTransition;

    /** @brief Persistent RedundancyPriority dbus object */
    std::unique_ptr<RedundancyPriority> redundancyPriority;

    /** @brief Persistent ActivationProgress dbus object */
    std::unique_ptr<ActivationProgress> activationProgress;

    /** @brief Apply time */
    ApplyTimeIntf::RequestedApplyTimes applyTime;

    /** @brief Used to subscribe to dbus systemd signals **/
    sdbusplus::match systemdSignals;

    /** @brief Peer-update object paths (exposing ActivationBlocksTransition)
     *         still blocking this deferred reboot. The reboot is issued once
     *         this becomes empty. */
    std::set<std::string> pendingBlockers;

    /** @brief True only while the pending set is being seeded, so the watch
     *         callbacks do not issue a reboot before the initial set is known.
     */
    bool deferralSeeding = false;

    /** @brief Watches for ActivationBlocksTransition objects appearing while a
     *         reboot is deferred, so a peer update that starts mid-wait keeps
     *         the reboot deferred. */
    std::unique_ptr<sdbusplus::bus::match_t> blockerAddedMatch;

    /** @brief Watches for ActivationBlocksTransition objects disappearing, so
     *         the reboot is issued once the last peer update completes. */
    std::unique_ptr<sdbusplus::bus::match_t> blockerRemovedMatch;

    /** @brief One-shot guard so exactly one reboot is issued, no matter whether
     *         the watch callback or the timeout gets there first. */
    bool rebootIssued = false;

    /** @brief Shared "still alive" flag for the detached timeout task, so it
     *         never touches this Activation after it has been destroyed. */
    std::shared_ptr<bool> rebootDeferralAlive;

    /** @brief While a reboot is deferred, exposes RebootBlocksNewUpdates so
     *         other updaters refuse to start new updates until the pending BMC
     *         reboot has happened. Reset when the reboot is issued. */
    std::unique_ptr<RebootBlocksNewUpdatesInherit> rebootBlocksNewUpdates;

    /** @brief Tracks whether the read-write volume has been created as
     * part of the activation process. **/
    bool rwVolumeCreated = false;

    /** @brief Tracks whether the read-only volume has been created as
     * part of the activation process. **/
    bool roVolumeCreated = false;

    /** @brief Tracks if the service that updates the U-Boot environment
     *         variables has completed. **/
    bool ubootEnvVarsUpdated = false;

#ifdef WANT_SIGNATURE_VERIFY
  private:
    /** @brief Verify signature of the images.
     *
     * @param[in] imageDir - The path of images to verify
     * @param[in] confDir - The path of configs for verification
     *
     * @return true if verification successful and false otherwise
     */
    static bool verifySignature(const fs::path& imageDir,
                                const fs::path& confDir);

    /** @brief Called when image verification fails. */
    void onVerifyFailed();
#endif
};

} // namespace updater
} // namespace software
} // namespace phosphor
