#pragma once

#include <sdbusplus/bus.hpp>

namespace phosphor
{
namespace software
{
namespace updater
{

class Activation;

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
    Flash(sdbusplus::bus::bus& bus, std::string& versionId,
          Activation& parent) :
        bus(bus),
        versionId(versionId), parent(parent){};

    /**
     * @brief Writes the image file(s) to flash
     */
    void write();

    /**
     * @brief Takes action on service state changes that update the flash
     */
    void monitorStateChanges(sdbusplus::message::message& msg);

  private:
    /** @brief Persistent sdbusplus DBus bus connection. */
    sdbusplus::bus::bus& bus;

    /** @brief Software version id */
    std::string versionId;

    /** @brief Actviation Object. */
    Activation& parent;
};

} // namespace updater
} // namespace software
} // namespace phosphor
