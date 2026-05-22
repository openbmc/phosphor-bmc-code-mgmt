#include "software_utils.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::utils
{

bool unTar(int imageFd, const std::string& extractDirPath)
{
    info("Extracting archive to: {DIR}", "DIR", extractDirPath);

    pid_t pid = fork();
    if (pid < 0)
    {
        error("fork failed: {ERRNO}", "ERRNO", errno);
        return false;
    }
    if (pid == 0)
    {
        dup2(imageFd, STDIN_FILENO);
        // Pass tar arguments directly to exec to avoid any shell
        // interpretation of extractDirPath.
        execlp("tar", "tar", "-xf", "-", "-C", extractDirPath.c_str(),
               "--no-same-owner", nullptr);
        _exit(127);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        error("tar failed (status {STATUS}) extracting to {DIR}", "STATUS",
              status, "DIR", extractDirPath);
        return false;
    }
    return true;
}

} // namespace phosphor::software::utils
