#pragma once

#include <systemd/sd-event.h>
#include "version.hpp"
#include "image_manager.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

/** @class Watch
 *
 *  @brief Adds inotify watch on software image upload directory
 *
 *  The inotify watch is hooked up with sd-event, so that on call back,
 *  appropriate actions related to a software image upload can be taken.
 */
class Watch
{
    public:
        /** @brief ctor - hook inotify watch with sd-event
         *
         *  @param[in] loop - sd-event object
         *  @param[in] bus - The Dbus bus object
         */
        Watch(sd_event* loop, sdbusplus::bus::bus& bus);

        Watch(const Watch&) = delete;
        Watch& operator=(const Watch&) = delete;
        Watch(Watch&&) = default;
        Watch& operator=(Watch&&) = default;

        /** @brief dtor - remove inotify watch and close fd's
         */
        ~Watch();

        /** @brief The software image manager class. */
        Manager manager;

    private:
        /** @brief sd-event callback
         *
         *  @param[in] s - event source, floating (unused) in our case
         *  @param[in] fd - inotify fd
         *  @param[in] revents - events that matched for fd
         *  @param[in] userdata - pointer to Watch object
         *  @returns 0 on success, -1 on fail
         */
        static int callback(sd_event_source* s,
                            int fd,
                            uint32_t revents,
                            void* userdata);

        /** @brief image upload directory watch descriptor */
        int wd = -1;

        /** @brief inotify file descriptor */
        int fd = -1;
};

} // namespace manager
} // namespace software
} // namespace phosphor
