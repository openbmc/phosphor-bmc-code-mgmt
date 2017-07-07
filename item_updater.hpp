#pragma once

#include <sdbusplus/server.hpp>
#include "activation.hpp"
#include "version.hpp"
#include <xyz/openbmc_project/Common/FactoryReset/server.hpp>

namespace phosphor
{
namespace software
{
namespace updater
{

using ItemUpdaterInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Common::server::FactoryReset>;

namespace MatchRules = sdbusplus::bus::match::rules;

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
         * @param[in] bus    - The Dbus bus object
         */
        ItemUpdater(sdbusplus::bus::bus& bus, const std::string& path) :
                    ItemUpdaterInherit(bus, path.c_str()),
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
            processBMCImage();
        };

    /** @brief Sets the given priority free by incrementing
     *  any existing priority with the same value by 1
     *
     *  @param[in] value - The priority that needs to be set free.
     *
     *  @return None
     */
    void freePriority(uint8_t value);

    /**
     * @brief Create and populate the active BMC Version.
     */
    void processBMCImage();

    /**
     * @brief Erase specified entry d-bus object
     *        if Action property is not set to Active
     *
     * @param[in] entryId - unique identifier of the entry
     */
    void erase(std::string entryId);

    private:
        /** @brief Callback function for Software.Version match.
         *  @details Creates an Activation dbus object.
         *
         * @param[in]  msg       - Data associated with subscribed signal
         */
        void createActivation(sdbusplus::message::message& msg);

        /**
         * @brief Validates the presence of SquashFS iamge in the image dir.
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

        /** @brief Persistent sdbusplus DBus bus connection. */
        sdbusplus::bus::bus& bus;

        /** @brief Persistent map of Activation dbus objects and their
          * version id */
        std::map<std::string, std::unique_ptr<Activation>> activations;

        /** @brief Persistent map of Version dbus objects and their
          * version id */
        std::map<std::string, std::unique_ptr<phosphor::software::
            manager::Version>> versions;

        /** @brief sdbusplus signal match for Software.Version */
        sdbusplus::bus::match_t versionMatch;

        /** @brief Clears read only partition for
          * given Activation dbus object.
          *
          * @param[in]  versionId - The version id.
          */
        void removeReadOnlyPartition(std::string versionId);

        /** @brief Clears read write partition for
          * given Activation dbus object.
          *
          * @param[in]  versionId - The version id.
          */
        void removeReadWritePartition(std::string versionId);

};



} // namespace updater
} // namespace software
} // namespace phosphor
