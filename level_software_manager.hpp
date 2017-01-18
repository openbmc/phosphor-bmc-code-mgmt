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
        /** @brief Constructs Level Software Manager
         *
         * @param[in] bus       - The Dbus bus object
         * @param[in] objPath   - The Dbus object path
         */
        Level(sdbusplus::bus::bus& bus,
                const char* objPath) :
                sdbusplus::server::object::object<
                    sdbusplus::xyz::openbmc_project::Software::server::Level>(
                            bus, objPath) {};

        static std::string getVersion();

};

} // namespace manager
} // namespace software
} // namespace phosphor
