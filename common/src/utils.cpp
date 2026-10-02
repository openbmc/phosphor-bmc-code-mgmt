#include "common/include/utils.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

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
    ssize_t n = write(exitWriteFd, &exitCode, sizeof(exitCode));
    close(exitWriteFd);
    _exit((n == sizeof(exitCode)) ? 0 : 1);
}

static sdbusplus::async::task<bool> runParent(
    sdbusplus::async::context& ctx, pid_t pid, int exitReadFd, int resultReadFd,
    std::optional<std::reference_wrapper<std::string>> result,
    const std::string& cmd, AsyncSystemOutputCallback outputCallback)
{
    if (result)
    {
        result->get().clear();
    }

    if (resultReadFd != -1)
    {
        auto resultFdio =
            std::make_unique<sdbusplus::async::fdio>(ctx, resultReadFd);

        while (true)
        {
            co_await resultFdio->next();

            char buffer[1024];
            ssize_t n = read(resultReadFd, buffer, sizeof(buffer));

            if (n <= 0)
            {
                break;
            }

            if (result)
            {
                result->get().append(buffer, n);
            }

            if (outputCallback)
            {
                outputCallback(std::string_view(buffer, n));
            }
        }

        resultFdio.reset();
        close(resultReadFd);
    }

    auto exitFdio = std::make_unique<sdbusplus::async::fdio>(ctx, exitReadFd);

    co_await exitFdio->next();

    int exitCode = -1;
    exitFdio.reset();

    ssize_t bytesRead = read(exitReadFd, &exitCode, sizeof(exitCode));
    close(exitReadFd);

    if (bytesRead != sizeof(exitCode))
    {
        error("Failed to read exit code from command {CMD}", "CMD", cmd);
        waitpid(pid, nullptr, WNOHANG);
        co_return false;
    }

    if (waitpid(pid, nullptr, 0) < 0)
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

sdbusplus::async::task<bool> asyncSystem(
    sdbusplus::async::context& ctx, const std::string& cmd,
    std::optional<std::reference_wrapper<std::string>> result,
    AsyncSystemOutputCallback outputCallback)
{
    int exitPipefd[2];
    if (pipe(exitPipefd) == -1)
    {
        error("Failed to create pipe for command: {CMD}", "CMD", cmd);
        co_return false;
    }

    const bool captureOutput =
        result.has_value() || static_cast<bool>(outputCallback);

    int resultPipefd[2] = {-1, -1};
    if (captureOutput && pipe(resultPipefd) == -1)
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

        if (captureOutput)
        {
            close(resultPipefd[0]);
        }

        runChild(exitPipefd[1], captureOutput ? resultPipefd[1] : -1, cmd);
    }
    else if (pid < 0)
    {
        error("Fork failed for command: {CMD}", "CMD", cmd);

        close(exitPipefd[0]);
        close(exitPipefd[1]);

        if (captureOutput)
        {
            close(resultPipefd[0]);
            close(resultPipefd[1]);
        }

        co_return false;
    }

    close(exitPipefd[1]);

    if (captureOutput)
    {
        close(resultPipefd[1]);
    }

    co_return co_await runParent(ctx, pid, exitPipefd[0],
                                 captureOutput ? resultPipefd[0] : -1, result,
                                 cmd, std::move(outputCallback));
}
