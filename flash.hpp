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
     * @brief Writes the image file(s) to flash
     */
    virtual void flashWrite() = 0;

    /**
     * @brief Taks action on service file state changes
     */
    virtual void onStateChanges(sdbusplus::message::message& msg) = 0;
};

} // namespace updater
} // namespace software
} // namespace phosphor
