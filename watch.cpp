#include <stdexcept>
#include <cstddef>
#include <cstring>
#include <string>
#include <sys/inotify.h>
#include <unistd.h>
#include <phosphor-logging/log.hpp>
#include "config.h"
#include "watch.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using namespace std::string_literals;

Watch::Watch(sd_event* loop)
{
    fd = inotify_init1(IN_NONBLOCK);
    if (-1 == fd)
    {
        throw std::runtime_error(
            "inotify_init1 failed, errno="s + std::strerror(errno));
    }

    wd = inotify_add_watch(fd, IMG_UPLOAD_DIR, IN_CREATE);
    if (-1 == wd)
    {
        throw std::runtime_error(
            "inotify_add_watch failed, errno="s + std::strerror(errno));
    }

    auto rc = sd_event_add_io(loop,
                              nullptr,
                              fd,
                              EPOLLIN,
                              callback,
                              this);
    if (0 > rc)
    {
        throw std::runtime_error(
            "failed to add to event loop, rc="s + std::strerror(-rc));
    }
}

Watch::~Watch()
{
    if ((-1 != fd) && (-1 != wd))
    {
        inotify_rm_watch(fd, wd);
        close(fd);
    }
}

int Watch::callback(sd_event_source* s,
                    int fd,
                    uint32_t revents,
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
        throw std::runtime_error(
            "failed to read inotify event, errno="s + std::strerror(errno));
    }

    auto offset = 0;
    while (offset < bytes)
    {
        auto event = reinterpret_cast<inotify_event*>(&buffer[offset]);
        if ((event->mask & IN_CREATE) && !(event->mask & IN_ISDIR))
        {
            // TODO via issue 1352 - invoke method (which takes uploaded
            // filepath) to construct software version d-bus objects.
            // For now, log the image filename.
            using namespace phosphor::logging;
            log<level::INFO>(event->name);
        }

        offset += offsetof(inotify_event, name) + event->len;
    }

    return 0;
}

} // namespace manager
} // namespace software
} // namespace phosphor
