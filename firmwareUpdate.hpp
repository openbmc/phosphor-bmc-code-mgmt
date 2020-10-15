#pragma once

#include "xyz/openbmc_project/Software/FirmwareUpdate/server.hpp"

#include <sdbusplus/bus.hpp>

#include <functional>
#include <string>

namespace phosphor
{
namespace software
{
namespace updater
{

using FirmwareUpdateInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::FirmwareUpdate>;

class FirmwareUpdate : public FirmwareUpdateInherit
{
  public:
    /** @brief Constructs FirmwareUpdate Software Manager
     *
     * @param[in] bus            - The D-Bus bus object
     * @param[in] objPath        - The D-Bus object path
     * @param[in] toBeUpdated    - The bool value
     */
    FirmwareUpdate(sdbusplus::bus::bus& bus, const std::string& objPath,
                    const bool toBeUpdated) :
        FirmwareUpdateInherit(bus, (objPath).c_str(), true),
        isUpdated(toBeUpdated)
    {
        // Set properties.
        firmwareUpdate(toBeUpdated);
        // Emit deferred signal.
        emit_object_added();
    }

  private:
    /** @brief This Version's version string */
    const bool isUpdated;
};

} // namespace updater
} // namespace software
} // namespace phosphor
