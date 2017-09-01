#pragma once

#include <sdbusplus/bus.hpp>
#include "xyz/openbmc_project/Software/Version/server.hpp"
#include "xyz/openbmc_project/Common/FilePath/server.hpp"
#include <functional>

namespace phosphor
{
namespace software
{
namespace manager
{

typedef std::function<void(std::string)> eraseFunc;

using VersionInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Version,
    sdbusplus::xyz::openbmc_project::Common::server::FilePath>;

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
         * @param[in] filePath       - The image filesystem path
         */
        Version(sdbusplus::bus::bus& bus,
                const std::string& objPath,
                const std::string& versionId,
                VersionPurpose versionPurpose,
                const std::string& filePath) : VersionInherit(
                    bus, (objPath).c_str(), true), versionStr(versionId)
        {
            // Set properties.
            purpose(versionPurpose);
            version(versionId);
            path(filePath);
            // Emit deferred signal.
            emit_object_added();
        }

        /**
         * @brief Read the manifest file to get the value of the key.
         *
         * @return The value of the key.
         **/
        static std::string getValue(const std::string& manifestFilePath,
                                    std::string key);

        /**
         * @brief Get the Version id.
         *
         * @return The id.
         **/
        static std::string getId(const std::string& version);

        /**
         * @brief Get the active bmc version identifier.
         *
         * @param[in] releaseFilePath - The Path to file which contains
         *                              the release version.
         *
         * @return The version identifier.
         */
        static std::string getBMCVersion(const std::string& releaseFilePath);

        /* @brief Check if this version matches the currently running version
         *
         * @return - Returns true if this version matches the currently running
         *           version.
         */
        bool isFunctional();

    private:
        /** @brief This Version's version string */
        const std::string versionStr;
};

} // namespace manager
} // namespace software
} // namespace phosphor
