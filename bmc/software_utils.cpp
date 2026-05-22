#include "software_utils.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::utils
{

bool unTar(int imageFd, const std::string& extractDirPath)
{
    std::string tarCmd = "tar -xf - -C " + extractDirPath + " --no-same-owner";
    info("Executing command: {CMD}", "CMD", tarCmd);

    pid_t pid = fork();
    if (pid < 0)
    {
        error("fork failed: {ERRNO}", "ERRNO", errno);
        return false;
    }
    if (pid == 0)
    {
        dup2(imageFd, STDIN_FILENO);
        execl("/bin/sh", "sh", "-c", tarCmd.c_str(), nullptr);
        _exit(127);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        error("tar failed (status {STATUS}): {CMD}", "STATUS", status, "CMD",
              tarCmd);
        return false;
    }
    return true;
}

} // namespace phosphor::software::utils
