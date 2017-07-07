#pragma once

#include <sdbusplus/bus.hpp>
#include "xyz/openbmc_project/Software/Version/server.hpp"
#include "xyz/openbmc_project/Object/Delete/server.hpp"
#include "xyz/openbmc_project/Common/FilePath/server.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{
class ItemUpdater;
} // updater
namespace manager
{

using VersionInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Version,
    sdbusplus::xyz::openbmc_project::Object::server::Delete,
    sdbusplus::xyz::openbmc_project::Common::server::FilePath>;

class Manager;

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
         * @param[in] parent         - The version's parent
         * @param[in] updater        - The version's updater
         */
        Version(sdbusplus::bus::bus& bus,
                const std::string& objPath,
                const std::string& versionId,
                VersionPurpose versionPurpose,
                const std::string& filePath,
                Manager *parent,
                phosphor::software::updater::ItemUpdater *updater) : VersionInherit(
                    bus, (objPath).c_str(), true),
               parent(parent),
               updater(updater)
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
         * @return The version identifier.
         */
        static std::string getBMCVersion();

        void delete_() override;

    private:
        /** @brief This entry's parent */
        Manager *parent;

        /** @brief This entry's parent */
        phosphor::software::updater::ItemUpdater *updater;

};

} // namespace manager
} // namespace software
} // namespace phosphor

