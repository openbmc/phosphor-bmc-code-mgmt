#include "common/include/utils.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <cerrno>
#include <cstdlib>

PHOSPHOR_LOG2_USING;

[[noreturn]] static void runChild(int exitWriteFd, int resultWriteFd,
                                  const std::string& cmd)
{
    if (resultWriteFd != -1)
    {
        dup2(resultWriteFd, STDOUT_FILENO);
        dup2(resultWriteFd, STDERR_FILENO);
        close(resultWriteFd);
    }

    int exitCode = std::system(cmd.c_str());
    ssize_t n;
    do
    {
        n = write(exitWriteFd, &exitCode, sizeof(exitCode));
    } while (n < 0 && errno == EINTR);
    close(exitWriteFd);
    _exit((n == sizeof(exitCode)) ? 0 : 1);
}

static sdbusplus::async::task<bool> runParent(
    sdbusplus::async::context& ctx, pid_t pid, int exitReadFd, int resultReadFd,
    std::optional<std::reference_wrapper<std::string>> result,
    const std::string& cmd)
{
    if (result)
    {
        auto& resStr = result->get();
        resStr.clear();
        char buffer[1024];
        auto resultFdio =
            std::make_unique<sdbusplus::async::fdio>(ctx, resultReadFd);
        while (true)
        {
            co_await resultFdio->next();
            ssize_t bytesRead = read(resultReadFd, buffer, sizeof(buffer));
            if (bytesRead > 0)
            {
                resStr.append(buffer, bytesRead);
                continue;
            }
            if (bytesRead == 0)
            {
                break;
            }
            if (errno == EINTR)
            {
                continue;
            }

            error("Failed to read command output {CMD}: {ERRNO}", "CMD", cmd,
                  "ERRNO", errno);
            resultFdio.reset();
            close(resultReadFd);
            close(exitReadFd);
            while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR)
            {}
            co_return false;
        }
        resultFdio.reset();
        close(resultReadFd);
    }
    else
    {
        auto fdio = std::make_unique<sdbusplus::async::fdio>(ctx, exitReadFd);
        co_await fdio->next();
    }

    int exitCode = -1;
    ssize_t bytesRead;
    do
    {
        bytesRead = read(exitReadFd, &exitCode, sizeof(exitCode));
    } while (bytesRead < 0 && errno == EINTR);
    close(exitReadFd);

    pid_t waitResult;
    do
    {
        waitResult = waitpid(pid, nullptr, 0);
    } while (waitResult < 0 && errno == EINTR);

    if (waitResult < 0)
    {
        error("waitpid failed for PID {PID} for command {CMD}", "PID", pid,
              "CMD", cmd);
        co_return false;
    }

    if (bytesRead != sizeof(exitCode))
    {
        error("Failed to read exit code from command {CMD}", "CMD", cmd);
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

sdbusplus::async::task<bool> asyncSystem(
    sdbusplus::async::context& ctx, const std::string& cmd,
    std::optional<std::reference_wrapper<std::string>> result)
{
    int exitPipefd[2];
    if (pipe(exitPipefd) == -1)
    {
        error("Failed to create pipe for command: {CMD}", "CMD", cmd);
        co_return false;
    }

    int resultPipefd[2] = {-1, -1};
    if (result && pipe(resultPipefd) == -1)
    {
        error("Failed to create pipe for command: {CMD}", "CMD", cmd);
        close(exitPipefd[0]);
        close(exitPipefd[1]);
        co_return false;
    }

    pid_t pid = fork();
    if (pid == 0)
    {
        close(exitPipefd[0]);
        if (result)
        {
            close(resultPipefd[0]);
        }
        runChild(exitPipefd[1], result ? resultPipefd[1] : -1, cmd);
    }
    else if (pid < 0)
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

    close(exitPipefd[1]);
    if (result)
    {
        close(resultPipefd[1]);
    }
    co_return co_await runParent(ctx, pid, exitPipefd[0],
                                 result ? resultPipefd[0] : -1, result, cmd);
}
