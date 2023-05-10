#include "config.h"

#include "sync_watch.hpp"

#include <sys/inotify.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <fstream>
#include <system_error>

namespace phosphor
{
namespace software
{
namespace manager
{

PHOSPHOR_LOG2_USING;

void SyncWatch::addInotifyWatch(const fs::path& path)
{
    auto wd = inotify_add_watch(inotifyFd, path.c_str(),
                                IN_CLOSE_WRITE | IN_DELETE);
    if (-1 == wd)
    {
        error("inotify_add_watch on {PATH} failed: {ERRNO}", "ERRNO", errno,
              "PATH", path);
        return;
    }

    fileMap[wd] = fs::path(path);
}

SyncWatch::SyncWatch(sd_event& loop,
                     std::function<int(int, fs::path&)> syncCallback) :
    inotifyFd(-1),
    syncCallback(syncCallback), loop(loop)
{
    auto fd = inotify_init1(IN_NONBLOCK);
    if (-1 == fd)
    {
        error("inotify_init1 failed: {ERRNO}", "ERRNO", errno);
        return;
    }
    inotifyFd = fd;

    auto rc = sd_event_add_io(&loop, nullptr, fd, EPOLLIN, callback, this);
    if (0 > rc)
    {
        error("failed to add to event loop: {RC}", "RC", rc);
        return;
    }

    std::error_code ec;
    auto syncfile = fs::path(SYNC_LIST_DIR_PATH) / SYNC_LIST_FILE_NAME;
    if (fs::exists(syncfile, ec))
    {
        std::string line;
        std::ifstream file(syncfile.c_str());
        while (std::getline(file, line))
        {
            addInotifyWatch(line);
        }
    }
}

SyncWatch::~SyncWatch()
{
    if (inotifyFd != -1)
    {
        close(inotifyFd);
    }
}

int SyncWatch::callback(sd_event_source* /* s */, int fd, uint32_t revents,
                        void* userdata)
{
    if (!(revents & EPOLLIN))
    {
        return 0;
    }

    constexpr auto maxBytes = 1024;
    uint8_t buffer[maxBytes];
    auto bytes = read(fd, buffer, maxBytes);
    if (0 > bytes)
    {
        return 0;
    }

    auto syncWatch = static_cast<SyncWatch*>(userdata);
    auto offset = 0;
    while (offset < bytes)
    {
        auto event = reinterpret_cast<inotify_event*>(&buffer[offset]);

        // Watch was removed, re-add it if file still exists.
        if (event->mask & IN_IGNORED)
        {
            std::error_code ec;
            if (fs::exists(syncWatch->fileMap[event->wd], ec))
            {
                syncWatch->addInotifyWatch(syncWatch->fileMap[event->wd]);
            }
            else
            {
                info("The inotify watch on {PATH} was removed", "PATH",
                     syncWatch->fileMap[event->wd]);
            }
            return 0;
        }

        // fileMap<wd, path>
        auto rc = syncWatch->syncCallback(event->mask,
                                          syncWatch->fileMap[event->wd]);
        if (rc)
        {
            return rc;
        }

        offset += offsetof(inotify_event, name) + event->len;
    }

    return 0;
}

} // namespace manager
} // namespace software
} // namespace phosphor
