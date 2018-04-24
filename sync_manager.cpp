#include <phosphor-logging/log.hpp>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <unistd.h>
#include "config.h"
#include "sync_manager.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using namespace phosphor::logging;

int Sync::processEntry(int mask, const std::string& entryPath)
{
    int status{};
    pid_t pid = fork();

    if (pid == 0)
    {
        auto dst = std::string{ALT_RWFS} + entryPath;
        if (mask & IN_CLOSE_WRITE)
        {
            execl("/usr/bin/rsync", "rsync", "-a", entryPath.c_str(),
                  dst.c_str(), nullptr);
            log<level::ERR>("Error occurred during the rsync call",
                            entry("ERRNO=%d", errno),
                            entry("PATH=%s", entryPath.c_str()));
            return -1;
        }
        else if (mask & IN_DELETE)
        {
            execl("/usr/bin/rsync", "rsync", "-a", "--delete",
                  entryPath.c_str(), dst.c_str(), nullptr);
            log<level::ERR>("Error occurred during the rsync delete call",
                            entry("ERRNO=%d", errno),
                            entry("PATH=%s", entryPath.c_str()));
            return -1;
        }
    }
    else if (pid > 0)
    {
        waitpid(pid, &status, 0);
    }
    else
    {
        log<level::ERR>("Error occurred during fork", entry("ERRNO=%d", errno));
        return -1;
    }

    return 0;
}

} // namespace manager
} // namespace software
} // namepsace phosphor
