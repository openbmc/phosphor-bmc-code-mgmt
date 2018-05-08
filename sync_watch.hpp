#pragma once

#include <experimental/filesystem>
#include <functional>
#include <systemd/sd-event.h>

namespace phosphor
{
namespace software
{
namespace manager
{

namespace fs = std::experimental::filesystem;

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
    SyncWatch(sd_event& loop, std::function<int(int, fs::path&)> syncCallback);

    SyncWatch(const SyncWatch&) = delete;
    SyncWatch& operator=(const SyncWatch&) = delete;
    SyncWatch(SyncWatch&&) = default;
    SyncWatch& operator=(SyncWatch&&) = default;

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

    /** @brief Map of file descriptors, watch descriptors, and file paths */
    using fd = int;
    using wd = int;
    std::map<fd, std::map<wd, fs::path>> fileMap;

    /** @brief The callback function for processing the inotify event */
    std::function<int(int, fs::path&)> syncCallback;
};

} // namespace manager
} // namespace software
} // namespace phosphor
