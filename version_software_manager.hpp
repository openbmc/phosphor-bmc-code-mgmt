#pragma once

#include <sdbusplus/bus.hpp>
#include "xyz/openbmc_project/Software/Version/server.hpp"
#include "xyz/openbmc_project/Software/Activation/server.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using VersionInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Version,
    sdbusplus::xyz::openbmc_project::Software::server::Activation>;

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
         * @note This constructor passes 'true' to the base class in order to
         *       defer dbus object registration until we can
         *       set our properties
         *
         * @param[in] bus        - The Dbus bus object
         * @param[in] objPath    - The Dbus object path
         */
        Version(sdbusplus::bus::bus& bus,
                const char* objPath) : VersionInherit(
                    bus, (std::string{objPath} + '/' + getId()).c_str(), true)
        {
            // Set properties.
            purpose(VersionPurpose::BMC);
            version(getVersion());
            activation(Activations::Active);
            // requestedActivation default is "None", so no need to set.

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
