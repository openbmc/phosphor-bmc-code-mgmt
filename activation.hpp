#pragma once

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>

namespace phosphor
{
namespace software
{
namespace updater
{

using ActivationInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Activation>;

/** @class Activation
 *  @brief OpenBMC activation software management implementation.
 *  @details A concrete implementation for
 *  xyz.openbmc_project.Software.Activation DBus API.
 */
class Activation : public ActivationInherit
{
    public:
        /** @brief Constructs Activation Software Manager
         *
         * @param[in] bus    - The Dbus bus object
         * @param[in] path   - The Dbus object path
         * @param[in] versionId  - The software version id
         * @param[in] activationStatus - The status of Activation
         */
        Activation(sdbusplus::bus::bus& bus, const std::string& path,
                   std::string& versionId,
                   sdbusplus::xyz::openbmc_project::Software::
                   server::Activation::Activations activationStatus) :
                   ActivationInherit(bus, path.c_str(), true),
                   bus(bus),
                   path(path),
                   versionId(versionId)
        {
            // Set Properties.
            activation(activationStatus);
            // Emit deferred signal.
            emit_object_added();
        }

        /** @brief Persistent sdbusplus DBus bus connection */
        sdbusplus::bus::bus& bus;

        /** @brief Persistent DBus object path */
        std::string path;

        /** @brief Version id */
        std::string versionId;
};

} // namespace updater
} // namespace software
} // namespace phosphor
