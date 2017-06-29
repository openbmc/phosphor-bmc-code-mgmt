#pragma once

#include <sdbusplus/bus.hpp>
#include "xyz/openbmc_project/Object/Delete/server.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using DeleteInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Object::server::Delete>;

/** @class Delete
 *  @brief OpenBMC delete object management implementation.
 *  @details A concrete implementation for xyz.openbmc_project.Object.Delete
 *  DBus API.
 */
class Delete : public DeleteInherit
{
    public:
        /** @brief Constructs Delete Object Manager
         *
         * @param[in] bus       - The Dbus bus object
         * @param[in] objPath   - The Dbus object path
         */
        Delete(sdbusplus::bus::bus& bus,
                const std::string& objPath) : DeleteInherit(
                    bus, (objPath).c_str()) {};

        /**
         * @brief Delete the specified image
         *
         **/
        void delete_() override;
};

} // namespace manager
} // namespace software
} // namespace phosphor

