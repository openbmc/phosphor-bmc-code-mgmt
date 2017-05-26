#pragma once
#include "version.hpp"
#include <sdbusplus/bus.hpp>
#include "xyz/openbmc_project/Software/Version/server.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using ManagerInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Version>;

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
                ManagerInherit(bus, (std::string{path} + '/' + 
                getBMCId()).c_str(), true), bus(bus)
        {
        // Set properties.
        purpose(VersionPurpose::BMC);
        version(getBMCVersion());

        // Emit deferred signal.
        emit_object_added();
        }

        /**
         * @brief Verify and untar the tarball. Verify the manifest file.
         *        Create and populate the version and filepath interfaces.
         *
         * @param[in]  tarballFilePath - Tarball path.
         * @param[out] result          - 0 if successful.
         */
         int processImage(const std::string& tarballFilePath);

    //private:
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

        /**
         * @brief Get the active bmc version identifier.
         *
         * @return The version identifier.
         */
        const std::string getBMCVersion() const;
        
        /**
         * @brief Get the Active BMC Version id.
         *
         * @return The id.
         **/
        const std::string getBMCId() const;

};

} // namespace manager
} // namespace software
} // namespace phosphor
