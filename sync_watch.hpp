#pragma once

#include <functional>
#include <systemd/sd-event.h>

namespace phosphor
{
namespace software
{
namespace manager
{

/** @class SyncWatch
 *
 *  @brief Adds inotify watch on persistent files to be synced
 *
 *  The inotify watch is hooked up with sd-event, so that on call back,
 *  appropriate actions related to syncing files can be taken.
 */
class SyncWatch
{
  public:
    /** @brief ctor - hook inotify watch with sd-event
     *
     *  @param[in] loop - sd-event object
     *  @param[in] syncCallback - The callback function for processing
     *                            files
     */
    SyncWatch(sd_event* loop, std::function<int(std::string&)> syncCallback);

    SyncWatch(const SyncWatch&) = delete;
    SyncWatch& operator=(const SyncWatch&) = delete;
    SyncWatch(SyncWatch&&) = delete;
    SyncWatch& operator=(SyncWatch&&) = delete;

    /** @brief dtor - remove inotify watch and close fd's
     */
    ~SyncWatch();

  private:
    /** @brief sd-event callback
     *
     *  @param[in] s - event source, floating (unused) in our case
     *  @param[in] fd - inotify fd
     *  @param[in] revents - events that matched for fd
     *  @param[in] userdata - pointer to SyncWatch object
     *  @returns 0 on success, -1 on fail
     */
    static int callback(sd_event_source* s, int fd, uint32_t revents,
                        void* userdata);

    /** @brief watch descriptor */
    int wd = -1;

    /** @brief Map of file descriptors and file paths */
    std::map<int, std::string> fileMap;

    /** @brief The callback function for processing the inotify event */
    std::function<int(std::string&)> syncCallback;
};

} // namespace manager
} // namespace software
} // namespace phosphor
