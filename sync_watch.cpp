#include <experimental/filesystem>
#include <fstream>
#include <phosphor-logging/log.hpp>
#include <sys/inotify.h>
#include <unistd.h>
#include "config.h"
#include "sync_watch.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using namespace phosphor::logging;
namespace fs = std::experimental::filesystem;

SyncWatch::SyncWatch(sd_event* loop,
                     std::function<int(std::string&)> syncCallback) :
    syncCallback(syncCallback)
{
    auto syncfile = fs::path(SYNC_LIST_PATH) / SYNC_LIST_FILE_NAME;
    if (fs::exists(syncfile))
    {
        std::string line;
        std::ifstream file(syncfile.c_str());
        while (std::getline(file, line))
        {
            auto fd = inotify_init1(IN_NONBLOCK);
            if (-1 == fd)
            {
                log<level::ERR>("inotify_init1 failed",
                                entry("ERRNO=%d", errno),
                                entry("FILENAME=%s", line.c_str()),
                                entry("SYNCFILE=%s", syncfile.c_str()));
                continue;
            }

            wd = inotify_add_watch(fd, line.c_str(), IN_CLOSE_WRITE);
            if (-1 == wd)
            {
                log<level::ERR>("inotify_add_watch failed",
                                entry("ERRNO=%d", errno),
                                entry("FILENAME=%s", line.c_str()),
                                entry("SYNCFILE=%s", syncfile.c_str()));
                close(fd);
                continue;
            }

            auto rc =
                sd_event_add_io(loop, nullptr, fd, EPOLLIN, callback, this);
            if (0 > rc)
            {
                log<level::ERR>("failed to add to event loop",
                                entry("RC=%d", rc),
                                entry("FILENAME=%s", line.c_str()),
                                entry("SYNCFILE=%s", syncfile.c_str()));
                inotify_rm_watch(fd, wd);
                close(fd);
                continue;
            }

            fileMap.insert(std::make_pair(fd, line));
        }
    }
}

SyncWatch::~SyncWatch()
{
    if (-1 != wd)
    {
        for (const auto& i : fileMap)
        {
            inotify_rm_watch(i.first, wd);
            close(i.first);
        }
    }
}

int SyncWatch::callback(sd_event_source* s, int fd, uint32_t revents,
                        void* userdata)
{
    return 0;
}

} // namespace manager
} // namespace software
} // namespace phosphor
