#pragma once

#include <sdbusplus/bus.hpp>
#include "xyz/openbmc_project/Software/Version/server.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using VersionInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Version>;

/** @class Version
 *  @brief OpenBMC version software management implementation.
 *  @details A concrete implementation for xyz.openbmc_project.Software.Version
 *  DBus API.
 */
class Version : public VersionInherit
{
    public:
        /** @brief Constructs Version Software Manager
         *
         * @param[in] bus            - The Dbus bus object
         * @param[in] objPath        - The Dbus object path
         * @param[in] versionId      - The version identifier
         * @param[in] versionPurpose - The version purpose
         */
        Version(sdbusplus::bus::bus& bus,
                const std::string& objPath,
                const std::string& versionId,
                const VersionPurpose versionPurpose) : VersionInherit(
                    bus, (objPath).c_str(), true)
        {
            // Set properties.
            purpose(versionPurpose);
            version(versionId);

            // Emit deferred signal.
            emit_object_added();
        }

        /**
         * @brief Get the code version identifier.
         *
         * @return The version identifier.
         **/
        static std::string getVersion(const std::string& manifestFilePath);

        /**
         * @brief Get the code version purpose.
         *
         * @return The version purpose.
         **/
        static std::string getPurpose(const std::string& manifestFilePath);

        /**
         * @brief Get the Version id.
         *
         * @return The id.
         **/
        static std::string getId(const std::string& version);

        /**
         * @brief Read the manifest file to get the value of the key.
         *
         * @return The value of the key.
         **/
        static std::string readManifest(const std::string& manifestFilePath,
                                        const std::string& key);
};

} // namespace manager
} // namespace software
} // namespace phosphor

