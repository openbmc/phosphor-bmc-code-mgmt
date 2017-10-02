#pragma once

#include <sdbusplus/server.hpp>
#include "activation.hpp"
#include "version.hpp"
#include <xyz/openbmc_project/Common/FactoryReset/server.hpp>
#include <xyz/openbmc_project/Control/FieldMode/server.hpp>
#include "org/openbmc/Associations/server.hpp"
#include "xyz/openbmc_project/Collection/DeleteAll/server.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

using ItemUpdaterInherit = sdbusplus::server::object::object<
        sdbusplus::xyz::openbmc_project::Common::server::FactoryReset,
        sdbusplus::xyz::openbmc_project::Control::server::FieldMode,
        sdbusplus::org::openbmc::server::Associations,
        sdbusplus::xyz::openbmc_project::Collection::server::DeleteAll>;

namespace MatchRules = sdbusplus::bus::match::rules;
using VersionClass = phosphor::software::manager::Version;
using AssociationList =
        std::vector<std::tuple<std::string, std::string, std::string>>;

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
        ItemUpdater(sdbusplus::bus::bus& bus, const std::string& path) :
                    ItemUpdaterInherit(bus, path.c_str(), false),
                    bus(bus),
                    versionMatch(
                            bus,
                            MatchRules::interfacesAdded() +
                            MatchRules::path("/xyz/openbmc_project/software"),
                            std::bind(
                                    std::mem_fn(&ItemUpdater::createActivation),
                                    this,
                                    std::placeholders::_1))
        {
            setBMCInventoryPath();
            processBMCImage();
            restoreFieldModeStatus();
            emit_object_added();
        };

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

        /** @brief Removes an active association to the software image
         *
         * @param[in]  path - The path to remove the association from.
         */
        void removeActiveAssociation(const std::string& path);

        /** @brief Determine if the given priority is the lowest
         *
         *  @param[in] value - The priority that needs to be checked.
         *
         *  @return boolean corresponding to whether the given
         *      priority is lowest.
         */
        bool isLowestPriority(uint8_t value);

    /**
     * @brief Updates the uboot variables to point to BMC version with lowest
     *        priority, so that the system boots from this version on the
     *        next boot.
     */
    void resetUbootEnvVars();

    private:
        /** @brief Callback function for Software.Version match.
         *  @details Creates an Activation D-Bus object.
         *
         * @param[in]  msg       - Data associated with subscribed signal
         */
        void createActivation(sdbusplus::message::message& msg);

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
        sdbusplus::bus::bus& bus;

        /** @brief Persistent map of Activation D-Bus objects and their
          * version id */
        std::map<std::string, std::unique_ptr<Activation>> activations;

        /** @brief Persistent map of Version D-Bus objects and their
          * version id */
        std::map<std::string, std::unique_ptr<VersionClass>> versions;

        /** @brief sdbusplus signal match for Software.Version */
        sdbusplus::bus::match_t versionMatch;

        /** @brief This entry's associations */
        AssociationList assocs = {};

        /** @brief Clears read only partition for
         * given Activation D-Bus object.
         *
         * @param[in]  versionId - The version id.
         */
        void removeReadOnlyPartition(std::string versionId);
};

} // namespace updater
} // namespace software
} // namespace phosphor
