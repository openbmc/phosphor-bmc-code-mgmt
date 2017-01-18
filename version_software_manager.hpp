#pragma once
#include <iostream>

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
        /** @brief Constructs Version Software Manager
         *
         * @param[in] bus       - The Dbus bus object
         * @param[in] objPath   - The Dbus object path
         */
        Version(sdbusplus::bus::bus& bus,
                const char* objPath) :
                sdbusplus::server::object::object<
                    sdbusplus::xyz::openbmc_project::Software::server::Version>
                        (bus, (std::string{objPath} + '/' +
                            getId()).c_str()) {};

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
