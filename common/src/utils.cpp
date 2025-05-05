#include "common/include/utils.hpp"

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<int> asyncSystem(sdbusplus::async::context& ctx,
                                        const std::string& cmd)
// NOLINTEND(readability-static-accessed-through-instance)
{
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        perror("pipe failed");
        co_return -1;
    }

    pid_t pid = fork();
    if (pid == 0)
    {
        close(pipefd[0]);
        int exitCode = std::system(cmd.c_str());

        ssize_t status = write(pipefd[1], &exitCode, sizeof(exitCode));
        close(pipefd[1]);
        exit((status == sizeof(exitCode)) ? 0 : 1);
    }
    else if (pid > 0)
    {
        close(pipefd[1]);

        sdbusplus::async::fdio pipe_fdio(ctx, pipefd[0]);

        // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
        co_await pipe_fdio.next();
        // NOLINTEND(clang-analyzer-core.uninitialized.Branch)

        int status;
        if (waitpid(pid, &status, 0) < 0)
        {
            perror("waitpid failed");
            co_return -1;
        }
        close(pipefd[0]);

        co_return WEXITSTATUS(status);
    }
    else
    {
        perror("fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        co_return -1;
    }
}
