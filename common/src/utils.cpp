#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

sdbusplus::async::task<bool> asyncSystem(sdbusplus::async::context& ctx,
                                         const std::string& cmd)
{
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        debug("Failed to create pipe for command: {CMD}", "CMD", cmd);
        co_return false;
    }

    pid_t pid = fork();
    if (pid == 0)
    {
        close(pipefd[0]);

        int exitCode = std::system(cmd.c_str());
        ssize_t status = write(pipefd[1], &exitCode, sizeof(exitCode));

        close(pipefd[1]);
        _exit((status == sizeof(exitCode)) ? 0 : 1);
    }
    else if (pid > 0)
    {
        close(pipefd[1]);

        auto fdio = std::make_unique<sdbusplus::async::fdio>(ctx, pipefd[0]);

        if (!fdio)
        {
            debug("Failed to create fdio for command: {CMD}", "CMD", cmd);
            close(pipefd[0]);
            co_return false;
        }

        co_await fdio->next();

        int exitCode = -1;
        ssize_t bytesRead = read(pipefd[0], &exitCode, sizeof(exitCode));
        close(pipefd[0]);

        if (bytesRead != sizeof(exitCode))
        {
            debug("Failed to read exit code from command {CMD}", "CMD", cmd);
            co_return false;
        }

        int status;
        if (waitpid(pid, &status, 0) < 0)
        {
            debug("waitpid failed for PID {PID} for command {CMD}", "PID", pid,
                  "CMD", cmd);
            co_return false;
        }

        if (exitCode != 0)
        {
            debug("Command {CMD} exited with code {CODE}", "CMD", cmd, "CODE",
                  exitCode);
            co_return false;
        }

        debug("{CMD} executed successfully", "CMD", cmd);

        co_return true;
    }
    else
    {
        debug("Fork failed for command: {CMD}", "CMD", cmd);
        close(pipefd[0]);
        close(pipefd[1]);
        co_return false;
    }
}
