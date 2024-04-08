#pragma once

#include "activation.hpp"
#include "item_updater_helper.hpp"
#include "msl_verify.hpp"
#include "version.hpp"
#include "xyz/openbmc_project/Collection/DeleteAll/server.hpp"

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Common/FactoryReset/server.hpp>
#include <xyz/openbmc_project/Control/FieldMode/server.hpp>
#include <xyz/openbmc_project/Software/MinimumVersion/server.hpp>

#include <string>
#include <vector>

namespace phosphor
{
namespace software
{
namespace updater
{

using ActivationIntf =
    sdbusplus::xyz::openbmc_project::Software::server::Activation;
using ItemUpdaterInherit = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::common::FactoryReset,
    sdbusplus::server::xyz::openbmc_project::control::FieldMode,
    sdbusplus::server::xyz::openbmc_project::association::Definitions,
    sdbusplus::server::xyz::openbmc_project::collection::DeleteAll>;
using MinimumVersionInherit = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::software::MinimumVersion>;

namespace MatchRules = sdbusplus::bus::match::rules;
using VersionClass = phosphor::software::manager::Version;
using AssociationList =
    std::vector<std::tuple<std::string, std::string, std::string>>;

/** @class MinimumVersion
 *  @brief OpenBMC MinimumVersion implementation.
 *  @details A concrete implementation for
 *  xyz.openbmc_project.Software.MinimumVersion DBus API.
 */
class MinimumVersion : public MinimumVersionInherit
{
  public:
    /** @brief Constructs MinimumVersion
     *
     * @param[in] bus - The D-Bus bus object
     * @param[in] path - The D-bus object path
     */
    MinimumVersion(sdbusplus::bus_t& bus, const std::string& path) :
        MinimumVersionInherit(bus, path.c_str(), action::emit_interface_added)
    {}
};

/** @class ItemUpdater
 *  @brief Manages the activation of the BMC version items.
 */
class ItemUpdater : public ItemUpdaterInherit
{
  public:
    /*
     * @brief Types of Activation status for image validation.
     */
    enum class ActivationStatus
    {
        ready,
        invalid,
        active
    };

    /** @brief Constructs ItemUpdater
     *
     * @param[in] bus    - The D-Bus bus object
     */
    ItemUpdater(sdbusplus::bus_t& bus, const std::string& path,
                bool useUpdateDBusInterface) :
        ItemUpdaterInherit(bus, path.c_str(),
                           ItemUpdaterInherit::action::defer_emit),
        bus(bus), helper(bus)
    {
        if (!useUpdateDBusInterface)
        {
            versionMatch = std::make_unique<sdbusplus::bus::match_t>(
                bus,
                MatchRules::interfacesAdded() +
                    MatchRules::path("/xyz/openbmc_project/software"),
                std::bind(std::mem_fn(&ItemUpdater::createActivation), this,
                          std::placeholders::_1));
        }
        getRunningSlot();
        setBMCInventoryPath();
        processBMCImage();
        restoreFieldModeStatus();
#ifdef HOST_BIOS_UPGRADE
        createBIOSObject();
#endif

        if (minimum_ship_level::enabled())
        {
            minimumVersionObject = std::make_unique<MinimumVersion>(bus, path);
            minimumVersionObject->minimumVersion(
                minimum_ship_level::getMinimumVersion());
        }

        emit_object_added();
    };

    /** @brief Save priority value to persistent storage (flash and optionally
     *  a U-Boot environment variable)
     *
     *  @param[in] versionId - The Id of the version
     *  @param[in] value - The priority value
     *  @return None
     */
    void savePriority(const std::string& versionId, uint8_t value);

    /** @brief Sets the given priority free by incrementing
     *  any existing priority with the same value by 1
     *
     *  @param[in] value - The priority that needs to be set free.
     *  @param[in] versionId - The Id of the version for which we
     *                         are trying to free up the priority.
     *  @return None
     */
    void freePriority(uint8_t value, const std::string& versionId);

    /**
     * @brief Create and populate the active BMC Version.
     */
    void processBMCImage();

    /**
     * @brief Verifies the image at filepath and creates the version and
     * activation object. In case activation object already exists for the
     * specified id, update the activation status based on image verification.
     * @param[in] id - The unique identifier for the update.
     * @param[in] path - The object path for the relevant objects.
     * @param[in] version - The version of the image.
     * @param[in] purpose - The purpose of the image.
     * @param[in] extendedVersion The extended version of the image.
     * @param[in] filePath - The file path where the image is located.
     * @param[in] compatibleNames - The compatible name for the image.
     * @param[out] Activations - Whether the image is ready to activate or not.
     */
    ActivationIntf::Activations verifyAndCreateObjects(
        std::string& id, sdbusplus::message::object_path& path,
        std::string& version, VersionClass::VersionPurpose purpose,
        std::string& extendedVersion, std ::string& filePath,
        std::vector<std::string>& compatibleNames);

    /**
     * @brief Creates the activation object
     * @param[in] id - The unique identifier for the update.
     * @param[in] path - The object path for the activation object.
     * @param[in] applyTime - The apply time for the image
     */
    void createActivationWithApplyTime(
        std::string& id, sdbusplus::message::object_path& path,
        ApplyTimeIntf::RequestedApplyTimes applyTime);

    /**
     * @brief Request the activation for the specified update.
     * @param[in] id - The unique identifier for the update.
     * @param[out] bool - status for the action.
     */
    bool requestActivation(std::string& id);

    /**
     * @brief Change the activation status for the specified update.
     * @param[in] id - The unique identifier for the update.
     * @param[in] status - The activation status to set.
     * @param[out] bool - status for the action.
     */
    bool updateActivationStatus(std::string& id,
                                ActivationIntf::Activations status);

    /**
     * @brief Erase specified entry D-Bus object
     *        if Action property is not set to Active
     *
     * @param[in] entryId - unique identifier of the entry
     */
    void erase(std::string entryId);

    /**
     * @brief Deletes all versions except for the current one
     */
    void deleteAll();

    /** @brief Creates an active association to the
     *  newly active software image
     *
     * @param[in]  path - The path to create the association to.
     */
    void createActiveAssociation(const std::string& path);

    /** @brief Removes the associations from the provided software image path
     *
     * @param[in]  path - The path to remove the associations from.
     */
    void removeAssociations(const std::string& path);

    /** @brief Determine if the given priority is the lowest
     *
     *  @param[in] value - The priority that needs to be checked.
     *
     *  @return boolean corresponding to whether the given
     *      priority is lowest.
     */
    bool isLowestPriority(uint8_t value);

    /**
     * @brief Updates the U-Boot variables to point to the requested
     *        versionId, so that the systems boots from this version on
     *        the next reboot.
     *
     * @param[in] versionId - The version to point the system to boot from.
     */
    void updateUbootEnvVars(const std::string& versionId);

    /**
     * @brief Updates the uboot variables to point to BMC version with lowest
     *        priority, so that the system boots from this version on the
     *        next boot.
     */
    void resetUbootEnvVars();

    /** @brief Brings the total number of active BMC versions to
     *         ACTIVE_BMC_MAX_ALLOWED -1. This function is intended to be
     *         run before activating a new BMC version. If this function
     *         needs to delete any BMC version(s) it will delete the
     *         version(s) with the highest priority, skipping the
     *         functional BMC version.
     *
     * @param[in] caller - The Activation object that called this function.
     */
    void freeSpace(const Activation& caller);

    /** @brief Creates a updateable association to the
     *  "running" BMC software image
     *
     * @param[in]  path - The path to create the association.
     */
    void createUpdateableAssociation(const std::string& path);

    /** @brief Persistent map of Version D-Bus objects and their
     * version id */
    std::map<std::string, std::unique_ptr<VersionClass>> versions;

    /** @brief Vector of needed BMC images in the tarball*/
    std::vector<std::string> imageUpdateList;

    /** @breif The slot of running BMC image */
    uint32_t runningImageSlot = 0;

  private:
    /** @brief Callback function for Software.Version match.
     *  @details Creates an Activation D-Bus object.
     *
     * @param[in]  msg       - Data associated with subscribed signal
     */
    void createActivation(sdbusplus::message_t& msg);

    /**
     * @brief Validates the presence of SquashFS image in the image dir.
     *
     * @param[in]  filePath  - The path to the image dir.
     * @param[out] result    - ActivationStatus Enum.
     *                         ready if validation was successful.
     *                         invalid if validation fail.
     *                         active if image is the current version.
     *
     */
    ActivationStatus validateSquashFSImage(const std::string& filePath);

    /** @brief BMC factory reset - marks the read-write partition for
     * recreation upon reboot. */
    void reset() override;

    /**
     * @brief Enables field mode, if value=true.
     *
     * @param[in]  value  - If true, enables field mode.
     * @param[out] result - Returns the current state of field mode.
     *
     */
    bool fieldModeEnabled(bool value) override;

    /** @brief Sets the BMC inventory item path under
     *  /xyz/openbmc_project/inventory/system/chassis/. */
    void setBMCInventoryPath();

    /** @brief The path to the BMC inventory item. */
    std::string bmcInventoryPath;

    /** @brief Restores field mode status on reboot. */
    void restoreFieldModeStatus();

    /** @brief Creates a functional association to the
     *  "running" BMC software image
     *
     * @param[in]  path - The path to create the association to.
     */
    void createFunctionalAssociation(const std::string& path);

    /** @brief Persistent sdbusplus D-Bus bus connection. */
    sdbusplus::bus_t& bus;

    /** @brief The helper of image updater. */
    Helper helper;

    /** @brief Persistent map of Activation D-Bus objects and their
     * version id */
    std::map<std::string, std::unique_ptr<Activation>> activations;

    /** @brief sdbusplus signal match for Software.Version */
    std::unique_ptr<sdbusplus::bus::match_t> versionMatch;

    /** @brief This entry's associations */
    AssociationList assocs = {};

    /** @brief Clears read only partition for
     * given Activation D-Bus object.
     *
     * @param[in]  versionId - The version id.
     */
    void removeReadOnlyPartition(std::string versionId);

    /** @brief Copies U-Boot from the currently booted BMC chip to the
     *  alternate chip.
     */
    void mirrorUbootToAlt();

    /** @brief Check the required image files
     *
     * @param[in] filePath - BMC tarball file path
     * @param[in] imageList - Image filenames included in the BMC tarball
     * @param[out] result - Boolean
     *                      true if all image files are found in BMC tarball
     *                      false if one of image files is missing
     */
    bool checkImage(const std::string& filePath,
                    const std::vector<std::string>& imageList);

    /** @brief Persistent MinimumVersion D-Bus object */
    std::unique_ptr<MinimumVersion> minimumVersionObject;

#ifdef HOST_BIOS_UPGRADE
    /** @brief Create the BIOS object without knowing the version.
     *
     *  The object is created only to provide the DBus access so that an
     *  external service could set the correct BIOS version.
     *  On BIOS code update, the version is updated accordingly.
     */
    void createBIOSObject();

    /** @brief Persistent Activation D-Bus object for BIOS */
    std::unique_ptr<Activation> biosActivation;

  public:
    /** @brief Persistent Version D-Bus object for BIOS */
    std::unique_ptr<VersionClass> biosVersion;
#endif

    /** @brief Get the slot number of running image */
    void getRunningSlot();
};

} // namespace updater
} // namespace software
} // namespace phosphor
