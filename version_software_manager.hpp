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
         * @param[in] bus       - The Dbus bus object
         * @param[in] objPath   - The Dbus object path
         */
        Version(sdbusplus::bus::bus& bus,
                const char* objPath) :
            sdbusplus::server::object::object <
            sdbusplus::xyz::openbmc_project::Software::server::Version >
            (bus, objPath) {};

        /**
         * @brief Get the code version
         *
         * @return The version
         **/
        static const std::string getVersion();
};

} // namespace manager
} // namespace software
} // namespace phosphor
