#include "config.h"

#include "sync_watch.hpp"

#include <sys/inotify.h>
#include <unistd.h>

#include <experimental/filesystem>
#include <fstream>
#include <phosphor-logging/log.hpp>

namespace phosphor
{
namespace software
{
namespace manager
{

using namespace phosphor::logging;
namespace fs = std::experimental::filesystem;

SyncWatch::SyncWatch(sd_event& loop,
                     std::function<int(int, fs::path&)> syncCallback) :
    inotifyFd(-1),
    syncCallback(syncCallback)
{
    auto fd = inotify_init1(IN_NONBLOCK);
    if (-1 == fd)
    {
        log<level::ERR>("inotify_init1 failed", entry("ERRNO=%d", errno));
        return;
    }
    inotifyFd = fd;

    auto rc = sd_event_add_io(&loop, nullptr, fd, EPOLLIN, callback, this);
    if (0 > rc)
    {
        log<level::ERR>("failed to add to event loop", entry("RC=%d", rc));
        return;
    }

    auto syncfile = fs::path(SYNC_LIST_DIR_PATH) / SYNC_LIST_FILE_NAME;
    if (fs::exists(syncfile))
    {
        std::string line;
        std::ifstream file(syncfile.c_str());
        while (std::getline(file, line))
        {
            auto wd =
                inotify_add_watch(fd, line.c_str(), IN_CLOSE_WRITE | IN_DELETE);
            if (-1 == wd)
            {
                log<level::ERR>("inotify_add_watch failed",
                                entry("ERRNO=%d", errno),
                                entry("FILENAME=%s", line.c_str()),
                                entry("SYNCFILE=%s", syncfile.c_str()));
                continue;
            }

            fileMap[wd] = fs::path(line);
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

int SyncWatch::callback(sd_event_source* s, int fd, uint32_t revents,
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

        // fileMap<wd, path>
        auto rc =
            syncWatch->syncCallback(event->mask, syncWatch->fileMap[event->wd]);
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
