#pragma once

#include <sdbusplus/bus.hpp>
#include "xyz/openbmc_project/Software/Activation/server.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

/** @class Activation
 *  @brief OpenBMC activation software management implementation.
 *  @details A concrete implementation for
 *           xyz.openbmc_project.Software.Activation
 *  DBus API.
 */
class Activation : public sdbusplus::server::object::object<
                sdbusplus::xyz::openbmc_project::Software::server::Activation>
{
    public:
        /** @brief Constructs Activation Software Manager
         *
         * @note This constructor passes 'true' to the base class in order to
         *       defer dbus object registration until we can
         *       set our properties
         *
         * @param[in] bus        - The Dbus bus object
         * @param[in] objPath    - The Dbus object path
         */
        Activation(sdbusplus::bus::bus& bus,
                const char* objPath) :
                sdbusplus::server::object::object<
                    sdbusplus::xyz::openbmc_project::Software::server::
                        Activation>(bus, objPath, true)
        {
            activation(phosphor::software::manager::Activation::
                    Activations::Active);
            // requestedActivation default is "None", so no need to set.

            // Emit deferred signal.
            emit_object_added();
        }
};

} // namespace manager
} // namespace software
} // namespace phosphor
