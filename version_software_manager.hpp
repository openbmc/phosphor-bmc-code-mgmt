#pragma once

#include <sdbusplus/bus.hpp>
#include "xyz/openbmc_project/Software/Version/server.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

/** @class Version
 *  @brief OpenBMC version software management implementation.
 *  @details A concrete implementation for xyz.openbmc_project.Software.Version
 *  DBus API.
 */
class Version : public sdbusplus::server::object::object<
                sdbusplus::xyz::openbmc_project::Software::server::Version>
{
    public:
        std::string id;

        /** @brief Constructs Version Software Manager
         *
         * @note This constructor passes 'true' to the base class in order to
         *       defer dbus object registration until we can
         *       set our properties
         *
         * @param[in] bus        - The Dbus bus object
         * @param[in] objPath    - The Dbus object path
         */
        Version(sdbusplus::bus::bus& bus,
                const char* objPath) :
                sdbusplus::server::object::object<
                    sdbusplus::xyz::openbmc_project::Software::server::Version>
                        (bus, (std::string{objPath} + '/' +
                            getId(getVersion())).c_str(), true)
        {
            // Set properties.
            purpose(VersionPurpose::BMC);
            id = getId(version(getVersion()));

            // Emit deferred signal.
            emit_object_added();
        }

    private:
        /**
         * @brief Get the code version identifier.
         *
         * @return The version identifier.
         **/
        const std::string getVersion();

        /**
         * @brief Get the Software Version id.
         *
         * @return The id.
         **/
        const std::string getId(const std::string version);
};

} // namespace manager
} // namespace software
} // namespace phosphor
