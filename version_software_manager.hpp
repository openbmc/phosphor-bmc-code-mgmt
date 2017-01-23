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
class Version : public sdbusplus::server::object::object <
    sdbusplus::xyz::openbmc_project::Software::server::Version >
{
    public:
        /** @brief Constructs Version Software Manager
         *
         * @param[in] version - The version identifier
         * @param[in] purpose - The purpose of the version
         */
        struct Properties
        {
            std::string version;
            VersionPurpose purpose;
        };

        /** @brief Constructs Version Software Manager
         *
         * @note This constructor passes 'true' to the base class in order to
         *       defer dbus object registration until we can run
         *       determineInitialProperties() and set our properties
         *
         * @param[in] bus        - The Dbus bus object
         * @param[in] objPath    - The Dbus object path
         * @param[in] properties - Desired Version properties.
         */
        Version(sdbusplus::bus::bus& bus,
                const char* objPath,
                const Properties& properties) :
            sdbusplus::server::object::object <
            sdbusplus::xyz::openbmc_project::Software::server::Version >
            (bus, objPath, true)
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
         * @param[in] version - The version identifier
         * @param[in] purpose - The purpose of the version
         *
         * @return Will throw exceptions on failure
         **/
        void setInitialProperties(const std::string& version,
                                  VersionPurpose purpose);

        /**
         * @brief Set value of purpose
         *
         * @param[in] value - The purpose of the version
         **/
        VersionPurpose purpose(VersionPurpose value) override;

        /**
         * @brief Set value of version
         *
         * @param[in] value - The version identifier
         **/
        std::string version(const std::string value) override;

        /**
         * @brief Get the code version identifier
         *
         * @return The version identifier
         **/
        static const std::string getVersion();
};

} // namespace manager
} // namespace software
} // namespace phosphor
