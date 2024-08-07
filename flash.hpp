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
    Flash() = default;
    Flash(const Flash&) = delete;
    Flash& operator=(const Flash&) = delete;
    Flash(Flash&&) = default;
    Flash& operator=(Flash&&) = delete;

    /* Destructor */
    virtual ~Flash() = default;

    /**
     * @brief Writes the image file(s) to flash
     */
    virtual void flashWrite() = 0;

    /**
     * @brief Takes action when the state of the activation service file changes
     */
    virtual void onStateChanges(sdbusplus::message_t& msg) = 0;
};

} // namespace updater
} // namespace software
} // namespace phosphor
