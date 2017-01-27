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
class Activation : public sdbusplus::server::object::object <
    sdbusplus::xyz::openbmc_project::Software::server::Activation >
{
    public:
        /** @brief Activation properties
         *
         * @param[in] activation - The current Activation state of the
         *                         Software.Version.
         */
        struct Properties
        {
            Activations activation;
            RequestedActivations requestedActivation;
        };

        /** @brief Constructs Activation Software Manager
         *
         * @note This constructor passes 'true' to the base class in order to
         *       defer dbus object registration until we can run
         *       setInitialProperties() and set our properties
         *
         * @param[in] bus        - The Dbus bus object
         * @param[in] objPath    - The Dbus object path
         * @param[in] properties - Desired Activation properties.
         */
        Activation(sdbusplus::bus::bus& bus,
                   const char* objPath,
                   const Properties& properties) :
            sdbusplus::server::object::object <
            sdbusplus::xyz::openbmc_project::Software::server::Activation > (
                bus, objPath, true)
        {
            activation(properties.activation);
            requestedActivation(properties.requestedActivation);

            // Emit deferred signal.
            emit_object_added();
        }

        /**
         * @brief Set value of activation
         *
         * @param[in] value - The current Activation state of the
         *                    Software.Version.
         **/
        Activations activation(Activations value) override;

        /**
         * @brief Set value of requestedActivation
         *
         * @param[in] value - The desired Activation state of the
         *                    Software.Version.
         **/
        RequestedActivations requestedActivation(
            RequestedActivations value) override;
};

} // namespace manager
} // namespace software
} // namespace phosphor
