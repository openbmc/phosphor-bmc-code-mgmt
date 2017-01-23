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
        /** @brief Properties of Level Interface
         *
         * @param[in] version - The version of the level
         * @param[in] purpose - The purpose of the level
         */
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
            purpose(properties.purpose);
            version(properties.version);

            // Will throw exception on fail
            setInitialProperties(properties.version, properties.purpose);

            // Emit deferred signal.
            emit_object_added();
        }

        /**
         * @brief Set initial properties
         *
         * @param[in] version - The version of the level
         * @param[in] purpose - The purpose of the level
         *
         * @return Will throw exceptions on failure
         **/
        void setInitialProperties(const std::string& version,
            const LevelPurpose& purpose);

        /**
         * @brief Set value of Purpose
         *
         * @param[in] value - The purpose of the level
         **/
        LevelPurpose purpose(const LevelPurpose value) override;

        /**
         * @brief Set value of Version
         *
         * @param[in] value - The version of the level
         **/
        std::string version(const std::string value) override;

        static std::string getVersion();

};

} // namespace manager
} // namespace software
} // namespace phosphor
