#include "common/include/utils.hpp"

sdbusplus::async::task<int> asyncSystem(sdbusplus::async::context& ctx,
                                        const std::string& cmd)
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
        _exit((status == sizeof(exitCode)) ? 0 : 1);
    }
    else if (pid > 0)
    {
        close(pipefd[1]);

        auto fdio = std::make_unique<sdbusplus::async::fdio>(ctx, pipefd[0]);

        if (!fdio)
        {
            perror("fdio creation failed");
            close(pipefd[0]);
            co_return -1;
        }

        co_await fdio->next();

        int exitCode = -1;
        ssize_t bytesRead = read(pipefd[0], &exitCode, sizeof(exitCode));
        close(pipefd[0]);

        if (bytesRead != sizeof(exitCode))
        {
            perror("read failed or incomplete");
            co_return -1;
        }

        int status;
        if (waitpid(pid, &status, 0) < 0)
        {
            perror("waitpid failed");
            co_return -1;
        }

        co_return exitCode;
    }
    else
    {
        perror("fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        co_return -1;
    }
}
