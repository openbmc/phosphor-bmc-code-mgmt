#pragma once

#include <sdbusplus/bus.hpp>
#include "xyz/openbmc_project/Software/Version/server.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using BMCVersionInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Version>;

/** @class BMCVersion
 *  @brief OpenBMC version software management implementation
 *         for the active BMC software image.
 *  @details A concrete implementation for xyz.openbmc_project.Software.Version
 *           DBus API.
 */
class BMCVersion : public BMCVersionInherit
{
    public:
        /** @brief Constructs BMC Version Software Manager for
         *         the active BMC software image.
         *
         * @note This constructor passes 'true' to the base class in order to
         *       defer dbus object registration until we can
         *       set our properties
         *
         * @param[in] bus        - The Dbus bus object
         * @param[in] objPath    - The Dbus object path
         */
        BMCVersion(sdbusplus::bus::bus& bus,
                   const char* objPath) : BMCVersionInherit(
                       bus, (std::string{objPath} + '/' + getId()).c_str(),
                       true)
        {
            // Set properties.
            purpose(VersionPurpose::BMC);
            version(getVersion());

            // Emit deferred signal.
            emit_object_added();
        }

    private:
        /**
         * @brief Get the code version identifier.
         *
         * @return The version identifier.
         **/
        const std::string getVersion() const;

        /**
         * @brief Get the Software Version id.
         *
         * @return The id.
         **/
        const std::string getId() const;
};

} // namespace manager
} // namespace software
} // namespace phosphor
