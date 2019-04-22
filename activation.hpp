#pragma once

#include "config.h"

#include "flash.hpp"
#include "org/openbmc/Associations/server.hpp"
#include "xyz/openbmc_project/Software/ActivationProgress/server.hpp"
#include "xyz/openbmc_project/Software/RedundancyPriority/server.hpp"

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/ActivationBlocksTransition/server.hpp>

#ifdef WANT_SIGNATURE_VERIFY
#include <experimental/filesystem>
#endif

namespace phosphor
{
namespace software
{
namespace updater
{

#ifdef WANT_SIGNATURE_VERIFY
namespace fs = std::experimental::filesystem;
#endif

using AssociationList =
    std::vector<std::tuple<std::string, std::string, std::string>>;
using ActivationInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Activation,
    sdbusplus::org::openbmc::server::Associations>;
using ActivationBlocksTransitionInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::
        ActivationBlocksTransition>;
using RedundancyPriorityInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::RedundancyPriority>;
using ActivationProgressInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::ActivationProgress>;

constexpr auto applyTimeImmediate =
    "xyz.openbmc_project.Software.ApplyTime.RequestedApplyTimes.Immediate";
constexpr auto applyTimeOnReset =
    "xyz.openbmc_project.Software.ApplyTime.RequestedApplyTimes.OnReset";
constexpr auto applyTimeIntf = "xyz.openbmc_project.Software.ApplyTime";
constexpr auto dbusPropIntf = "org.freedesktop.DBus.Properties";
constexpr auto applyTimeObjPath = "/xyz/openbmc_project/software/apply_time";
constexpr auto applyTimeProp = "RequestedApplyTime";
constexpr auto bmcStateIntf = "xyz.openbmc_project.State.BMC";
constexpr auto bmcStateObjPath = "/xyz/openbmc_project/state/bmc0";
constexpr auto bmcStateRebootProp = "RequestedBMCTransition";
constexpr auto bmcStateNewVal =
    "xyz.openbmc_project.State.BMC.Transition.Reboot";
constexpr auto mapperBusIntf = "xyz.openbmc_project.ObjectMapper";
constexpr auto mapperObjPath = "/xyz/openbmc_project/object_mapper";
constexpr auto rebootWaitTime = 5;

namespace sdbusRule = sdbusplus::bus::match::rules;

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
    RedundancyPriority(sdbusplus::bus::bus& bus, const std::string& path,
                       Activation& parent, uint8_t value,
                       bool freePriority = true) :
        RedundancyPriorityInherit(bus, path.c_str(), true),
        parent(parent), bus(bus), path(path)
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

        std::vector<std::string> interfaces({interface});
        bus.emit_interfaces_added(path.c_str(), interfaces);
    }

    ~RedundancyPriority()
    {
        std::vector<std::string> interfaces({interface});
        bus.emit_interfaces_removed(path.c_str(), interfaces);
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

  private:
    // TODO Remove once openbmc/openbmc#1975 is resolved
    static constexpr auto interface =
        "xyz.openbmc_project.Software.RedundancyPriority";
    sdbusplus::bus::bus& bus;
    std::string path;
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
    ActivationBlocksTransition(sdbusplus::bus::bus& bus,
                               const std::string& path) :
        ActivationBlocksTransitionInherit(bus, path.c_str(), true),
        bus(bus), path(path)
    {
        std::vector<std::string> interfaces({interface});
        bus.emit_interfaces_added(path.c_str(), interfaces);
        enableRebootGuard();
    }

    ~ActivationBlocksTransition()
    {
        std::vector<std::string> interfaces({interface});
        bus.emit_interfaces_removed(path.c_str(), interfaces);
        disableRebootGuard();
        // Reboot the BMC if the apply time value is immediate
        if (checkApplyTimeImmediate() == true)
        {
            rebootBmc();
        }
    }

  private:
    // TODO Remove once openbmc/openbmc#1975 is resolved
    static constexpr auto interface =
        "xyz.openbmc_project.Software.ActivationBlocksTransition";
    sdbusplus::bus::bus& bus;
    std::string path;

    /** @brief Enables a Guard that blocks any BMC reboot commands */
    void enableRebootGuard();

    /** @brief Disables any guard that was blocking the BMC reboot */
    void disableRebootGuard();

    /**
     * @brief Get the bus service
     *
     * @return the bus service as a string
     **/
    std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                           const std::string& interface);

    /**
     * @brief Determine the configured image apply time value
     *
     * @return true if the image apply time value is immediate
     **/
    bool checkApplyTimeImmediate();

    /**
     * @brief Reboot the bmc depending on image apply time value
     *
     * @return none
     **/
    void rebootBmc();
};

class ActivationProgress : public ActivationProgressInherit
{
  public:
    /** @brief Constructs ActivationProgress.
     *
     * @param[in] bus    - The Dbus bus object
     * @param[in] path   - The Dbus object path
     */
    ActivationProgress(sdbusplus::bus::bus& bus, const std::string& path) :
        ActivationProgressInherit(bus, path.c_str(), true), bus(bus), path(path)
    {
        progress(0);
        std::vector<std::string> interfaces({interface});
        bus.emit_interfaces_added(path.c_str(), interfaces);
    }

    ~ActivationProgress()
    {
        std::vector<std::string> interfaces({interface});
        bus.emit_interfaces_removed(path.c_str(), interfaces);
    }

  private:
    // TODO Remove once openbmc/openbmc#1975 is resolved
    static constexpr auto interface =
        "xyz.openbmc_project.Software.ActivationProgress";
    sdbusplus::bus::bus& bus;
    std::string path;
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
    Activation(sdbusplus::bus::bus& bus, const std::string& path,
               ItemUpdater& parent, std::string& versionId,
               sdbusplus::xyz::openbmc_project::Software::server::Activation::
                   Activations activationStatus,
               AssociationList& assocs) :
        ActivationInherit(bus, path.c_str(), true),
        bus(bus), path(path), parent(parent), versionId(versionId),
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
    RequestedActivations
        requestedActivation(RequestedActivations value) override;

    /** @brief Overloaded write flash function */
    void flashWrite() override;

    /** @brief Overloaded function that acts on service file state changes */
    void onStateChanges(sdbusplus::message::message&) override;

    /** @brief Check if systemd state change is relevant to this object
     *
     * Instance specific interface to handle the detected systemd state
     * change
     *
     * @param[in]  msg       - Data associated with subscribed signal
     *
     */
    void unitStateChange(sdbusplus::message::message& msg);

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

    /** @brief Persistent sdbusplus DBus bus connection */
    sdbusplus::bus::bus& bus;

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

    /** @brief Used to subscribe to dbus systemd signals **/
    sdbusplus::bus::match_t systemdSignals;

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
    bool verifySignature(const fs::path& imageDir, const fs::path& confDir);

    /** @brief Called when image verification fails. */
    void onVerifyFailed();
#endif
};

} // namespace updater
} // namespace software
} // namespace phosphor
