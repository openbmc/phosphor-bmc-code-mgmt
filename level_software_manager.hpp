#pragma once

#include <sdbusplus/bus.hpp>
#include "xyz/openbmc_project/Software/Level/server.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

/** @class Level
 *  @brief OpenBMC level software management implementation.
 *  @details A concrete implementation for xyz.openbmc_project.Software.Level
 *  DBus API.
 */
class Level : public sdbusplus::server::object::object<
                sdbusplus::xyz::openbmc_project::Software::server::Level>
{
    public:
        struct Properties
        {
            std::string version;
            LevelPurpose purpose;
        };

        /** @brief Constructs Level Software Manager
         *
         * @note This constructor passes 'true' to the base class in order to
         *       defer dbus object registration until we can run
         *       determineInitialProperties() and set our properties
         *
         * @param[in] bus        - The Dbus bus object
         * @param[in] objPath    - The Dbus object path
         * @param[in] properties - Desired Level properties.
         */
        Level(sdbusplus::bus::bus& bus,
                const char* objPath,
                const Properties& properties) :
                sdbusplus::server::object::object<
                    sdbusplus::xyz::openbmc_project::Software::server::Level>(
                            bus, objPath, true)
        {
            this->purpose(properties.purpose);
            this->version(properties.version);

            // Will throw exception on fail
            determineInitialProperties(properties.version, properties.purpose);
        }

        /**
         * @brief Determine initial properties and set internally
         *
         * @return Will throw exceptions on failure
         **/
        void determineInitialProperties(std::string version, LevelPurpose purpose);

        /** @brief Set value of Purpose */
        LevelPurpose purpose(LevelPurpose value) override;

        /** @brief Set value of Version */
        std::string version(std::string value) override;

};

} // namespace manager
} // namespace software
} // namespace phosphor
