#pragma once
#include <sdbusplus/server.hpp>
#include "version.hpp"
#include "xyz/openbmc_project/Collection/DeleteAll/server.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using ManagerInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Collection::server::DeleteAll>;

namespace MatchRules = sdbusplus::bus::match::rules;

/** @class Manager
 *  @brief Contains a map of Version dbus objects.
 *  @details The software image manager class that contains the Version dbus
 *           objects and their version ids.
 */
class Manager : public ManagerInherit
{
    public:
        /** @brief Constructs Manager Class
         *
         * @param[in] bus - The Dbus bus object
         */
        Manager(sdbusplus::bus::bus& bus, const std::string& path) :
                ManagerInherit(bus, path.c_str(), false),
                bus(bus),
                versionMatch(
                        bus,
                        MatchRules::interfacesRemoved() +
                        MatchRules::path(SOFTWARE_OBJPATH),
                        std::bind(
                                std::mem_fn(&Manager::removeVersion),
                                this,
                                std::placeholders::_1)){};

        /**
         * @brief Verify and untar the tarball. Verify the manifest file.
         *        Create and populate the version and filepath interfaces.
         *
         * @param[in]  tarballFilePath - Tarball path.
         * @param[out] result          - 0 if successful.
         */
         int processImage(const std::string& tarballFilePath);

        /**
         * @brief Erase specified entry d-bus object
         *        and deletes the image file.
         *
         * @param[in] entryId - unique identifier of the entry
         */
        void erase(std::string entryId);

        /**
         * @brief Erases all entry d-bus objects and deletes their respective
         *        image files.
         */
        void deleteAll();

    private:
        /** @brief Callback function for Software.Version match.
         *  @details Removes a version object.
         *
         * @param[in]  msg - Data associated with subscribed signal
         */
        void removeVersion(sdbusplus::message::message& msg);

        /** @brief Persistent map of Version dbus objects and their
          * version id */
        std::map<std::string, std::unique_ptr<Version>> versions;

        /** @brief Persistent sdbusplus DBus bus connection. */
        sdbusplus::bus::bus& bus;

        /** @brief sdbusplus signal match for Software.Version */
        sdbusplus::bus::match_t versionMatch;

        /**
         * @brief Untar the tarball.
         *
         * @param[in]  tarballFilePath - Tarball path.
         * @param[in]  extractDirPath  - Dir path to extract tarball ball to.
         * @param[out] result          - 0 if successful.
         */
        static int unTar(const std::string& tarballFilePath,
                         const std::string& extractDirPath);
};

} // namespace manager
} // namespace software
} // namespace phosphor
