#pragma once

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/ActivationBlocksTransition/server.hpp>
#include "xyz/openbmc_project/Software/RedundancyPriority/server.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

using ActivationInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Activation>;
using ActivationBlocksTransitionInherit = sdbusplus::server::object::object<
 sdbusplus::xyz::openbmc_project::Software::server::ActivationBlocksTransition>;
using RedundancyPriorityInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::RedundancyPriority>;

class ItemUpdater;
class Activation;
class RedundancyPriority;

/** @class RedundancyPriority
 *  @brief OpenBMC RedundancyPriority implementation
 *  @details A concrete implementation for
 *  xyz.openbmc_project.Software.RedundancyPriority DBus API.
 */
class RedundancyPriority : public RedundancyPriorityInherit
{
    public:
        /** @brief Constructs RedundancyPriority.
         *
         *  @param[in] bus    - The Dbus bus object
         *  @param[in] path   - The Dbus object path
         *  @param[in] parent - Parent object.
         *  @param[in] value  - The redundancyPriority value
         */
        RedundancyPriority(sdbusplus::bus::bus& bus,
                                   const std::string& path,
                                   Activation& parent,
                                   uint8_t value) :
                                   RedundancyPriorityInherit(bus,
                                   path.c_str(), true),
                                   parent(parent)
        {
            // Set Property
            priority(value);
            // Emit deferred signal.
            emit_object_added();
        }

        /** @brief Overloaded Priority property set function
         *
         *  @param[in] value - uint8_t
         *
         *  @return Success or exception thrown
         */
        uint8_t priority(uint8_t value) override;

        /** @brief Priority property get function
         *
         *  @returns uint8_t - The Priority value
         */
        using RedundancyPriorityInherit::priority;

        /** @brief Parent Object. */
        Activation& parent;
};

/** @class ActivationBlocksTransition
 *  @brief OpenBMC ActivationBlocksTransition implementation.
 *  @details A concrete implementation for
 *  xyz.openbmc_project.Software.ActivationBlocksTransition DBus API.
 */
class ActivationBlocksTransition : public ActivationBlocksTransitionInherit
{
    public:
        /** @brief Constructs ActivationBlocksTransition.
         *
         *  @param[in] bus    - The Dbus bus object
         *  @param[in] path   - The Dbus object path
         */
         ActivationBlocksTransition(sdbusplus::bus::bus& bus,
                                   const std::string& path) :
                   ActivationBlocksTransitionInherit(bus, path.c_str()) {}
};

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
         * @param[in] parent - Parent object.
         * @param[in] versionId  - The software version id
         * @param[in] activationStatus - The status of Activation
         */
        Activation(sdbusplus::bus::bus& bus, const std::string& path,
                   ItemUpdater& parent,
                   std::string& versionId,
                   sdbusplus::xyz::openbmc_project::Software::
                   server::Activation::Activations activationStatus) :
                   ActivationInherit(bus, path.c_str(), true),
                   bus(bus),
                   path(path),
                   parent(parent),
                   versionId(versionId)
        {
            // Set Properties.
            activation(activationStatus);
            // Emit deferred signal.
            emit_object_added();
        }

        /** @brief Overloaded Activation property setter function
         *
         * @param[in] value - One of Activation::Activations
         *
         * @return Success or exception thrown
         */
        Activations activation(Activations value) override;

        /** @brief Overloaded requestedActivation property setter function
         *
         * @param[in] value - One of Activation::RequestedActivations
         *
         * @return Success or exception thrown
         */
        RequestedActivations requestedActivation(RequestedActivations value)
                override;

        /** @brief Persistent sdbusplus DBus bus connection */
        sdbusplus::bus::bus& bus;

        /** @brief Persistent DBus object path */
        std::string path;

        /** @brief Parent Object. */
        ItemUpdater& parent;

        /** @brief Version id */
        std::string versionId;

        /** @brief Persistent ActivationBlocksTransition dbus object */
        std::unique_ptr<ActivationBlocksTransition> activationBlocksTransition;

        /** @brief Persistent RedundancyPriority dbus object */
        std::unique_ptr<RedundancyPriority> redundancyPriority;
};

} // namespace updater
} // namespace software
} // namespace phosphor
