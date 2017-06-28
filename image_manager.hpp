#pragma once
#include "version.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

namespace MatchRules = sdbusplus::bus::match::rules;

/** @class Manager
 *  @brief Contains a map of Version dbus objects.
 *  @details The software image manager class that contains the Version dbus
 *           objects and their version ids.
 */
class Manager
{
    public:
        /** @brief Constructs Manager Class
         *
         * @param[in] bus - The Dbus bus object
         */
//        Manager(sdbusplus::bus::bus& bus) : bus(bus) {};

        Manager(sdbusplus::bus::bus& bus) :
                    bus(bus),
                    versionMatch(
                            bus,
                            MatchRules::interfacesAdded() +
                            MatchRules::path("/xyz/openbmc_project/software"),
                            std::bind(
                                    std::mem_fn(&Manager::deleteImage),
                                    this,
                                    std::placeholders::_1))
        {
        };


        /**
         * @brief Verify and untar the tarball. Verify the manifest file.
         *        Create and populate the version and filepath interfaces.
         *
         * @param[in]  tarballFilePath - Tarball path.
         * @param[out] result          - 0 if successful.
         */
         int processImage(const std::string& tarballFilePath);

    private:
        /** @brief Callback function for Software.Version match.
         *  @details Creates an Activation dbus object.
         *
         * @param[in]  msg       - Data associated with subscribed signal
         */
        void deleteImage(sdbusplus::message::message& msg);

        /** @brief Persistent map of Version dbus objects and their
          * version id */
        std::map<std::string, std::unique_ptr<Version>> versions;

        /** @brief Persistent sdbusplus DBus bus connection. */
        sdbusplus::bus::bus& bus;

        /**
         * @brief Untar the tarball.
         *
         * @param[in]  tarballFilePath - Tarball path.
         * @param[in]  extractDirPath  - Dir path to extract tarball ball to.
         * @param[out] result          - 0 if successful.
         */
        static int unTar(const std::string& tarballFilePath,
                         const std::string& extractDirPath);

        /** @brief sdbusplus signal match for Software.Version */
        sdbusplus::bus::match_t versionMatch;

};

} // namespace manager
} // namespace software
} // namespace phosphor
