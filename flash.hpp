#pragma once

#include <sdbusplus/bus.hpp>

namespace phosphor
{
namespace software
{
namespace updater
{

/**
 *  @class Flash
 *  @brief Contains flash management functions.
 *  @details The software class that contains functions to interact
 *           with the flash.
 */
class Flash
{
  public:
    /**
     * @brief Constructs Flash Class
     *
     * @param[in] bus - The Dbus bus object
     * @param[in] versionId - The software version id
     */
    Flash(sdbusplus::bus::bus& bus, std::string& versionId) :
        bus(bus), versionId(versionId){};

    /**
     * @brief Writes the image file(s) to flash
     */
    void write();

  private:
    /** @brief Persistent sdbusplus DBus bus connection. */
    sdbusplus::bus::bus& bus;

    /** @brief Software version id */
    std::string versionId;
};

} // namespace updater
} // namespace software
} // namespace phosphor
