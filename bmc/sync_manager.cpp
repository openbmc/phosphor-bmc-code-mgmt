#include "config.h"

#include "sync_manager.hpp"

#include <sys/inotify.h>
#include <sys/wait.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace phosphor
{
namespace software
{
namespace manager
{

PHOSPHOR_LOG2_USING;
namespace fs = std::filesystem;

int Sync::processEntry(int mask, const fs::path& entryPath)
{
    if (!(mask & (IN_CLOSE_WRITE | IN_DELETE)))
    {
        debug("Ignoring inotify mask {MASK} for {PATH}", "MASK", lg2::hex,
              mask, "PATH", entryPath);
        return 0;
    }

    int status{};
    pid_t pid = fork();

    if (pid == 0)
    {
        fs::path dst(ALT_RWFS);
        dst /= entryPath.relative_path();

        // rsync needs an additional --delete argument to handle file deletions
        // so need to differentiate between the different file events.
        if (mask & IN_CLOSE_WRITE)
        {
            std::error_code ec;
            if (!(fs::exists(dst, ec)))
            {
                if (fs::is_directory(entryPath, ec))
                {
                    // Source is a directory, create it at the destination.
                    fs::create_directories(dst, ec);
                }
                else
                {
                    // Source is a file, create the directory where this file
                    // resides at the destination.
                    fs::create_directories(dst.parent_path(), ec);
                }
            }

            execl("/usr/bin/rsync", "rsync", "-aI", entryPath.c_str(),
                  dst.c_str(), nullptr);
        }
        else if (mask & IN_DELETE)
        {
            execl("/usr/bin/rsync", "rsync", "-a", "--delete",
                  entryPath.c_str(), dst.c_str(), nullptr);
        }

        // execl only returns on fail; a forked child must never return.
        error("Error ({ERRNO}) occurred during rsync on {PATH}, mask {MASK}",
              "ERRNO", errno, "PATH", entryPath, "MASK", lg2::hex, mask);
        _exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        waitpid(pid, &status, 0);
    }
    else
    {
        error("Error ({ERRNO}) occurred during fork", "ERRNO", errno);
        return -1;
    }

    return 0;
}

} // namespace manager
} // namespace software
} // namespace phosphor
