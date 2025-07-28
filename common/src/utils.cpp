#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

sdbusplus::async::task<bool> asyncSystem(
    sdbusplus::async::context& ctx, const std::string& cmd,
    std::optional<std::reference_wrapper<std::string>> result)
{
    int exitPipefd[2];
    int resultPipefd[2];

    if (pipe(exitPipefd) == -1 || (result && pipe(resultPipefd) == -1))
    {
        error("Failed to create pipe for command: {CMD}", "CMD", cmd);
        co_return false;
    }

    pid_t pid = fork();
    if (pid == 0)
    {
        close(exitPipefd[0]);

        if (result)
        {
            close(resultPipefd[0]);
            dup2(resultPipefd[1], STDOUT_FILENO);
            dup2(resultPipefd[1], STDERR_FILENO);
            close(resultPipefd[1]);
        }

        int exitCode = std::system(cmd.c_str());
        ssize_t status = write(exitPipefd[1], &exitCode, sizeof(exitCode));

        close(exitPipefd[1]);
        _exit((status == sizeof(exitCode)) ? 0 : 1);
    }
    else if (pid > 0)
    {
        close(exitPipefd[1]);

        if (result)
        {
            close(resultPipefd[1]);
        }

        auto fdio =
            std::make_unique<sdbusplus::async::fdio>(ctx, exitPipefd[0]);

        if (!fdio)
        {
            error("Failed to create fdio for command: {CMD}", "CMD", cmd);
            close(exitPipefd[0]);
            co_return false;
        }

        co_await fdio->next();

        if (result)
        {
            auto& resStr = result->get();
            resStr.clear();
            char buffer[1024];
            ssize_t n;
            while ((n = read(resultPipefd[0], buffer, sizeof(buffer))) > 0)
            {
                resStr.append(buffer, n);
            }
            close(resultPipefd[0]);
        }

        int exitCode = -1;
        ssize_t bytesRead = read(exitPipefd[0], &exitCode, sizeof(exitCode));
        close(exitPipefd[0]);

        if (bytesRead != sizeof(exitCode))
        {
            error("Failed to read exit code from command {CMD}", "CMD", cmd);
            co_return false;
        }

        int status;
        if (waitpid(pid, &status, 0) < 0)
        {
            error("waitpid failed for PID {PID} for command {CMD}", "PID", pid,
                  "CMD", cmd);
            co_return false;
        }

        if (exitCode != 0)
        {
            error("Command {CMD} exited with code {CODE}", "CMD", cmd, "CODE",
                  exitCode);
            co_return false;
        }

        debug("{CMD} executed successfully", "CMD", cmd);

        co_return true;
    }
    else
    {
        error("Fork failed for command: {CMD}", "CMD", cmd);
        close(exitPipefd[0]);
        close(exitPipefd[1]);
        if (result)
        {
            close(resultPipefd[0]);
            close(resultPipefd[1]);
        }
        co_return false;
    }
}
