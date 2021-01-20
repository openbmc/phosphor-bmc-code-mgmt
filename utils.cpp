#include "utils.hpp"

#include <unistd.h>

#include <phosphor-logging/log.hpp>

namespace utils
{

using namespace phosphor::logging;

std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                       const std::string& interface)
{
    auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                      MAPPER_BUSNAME, "GetObject");

    method.append(path);
    method.append(std::vector<std::string>({interface}));

    std::vector<std::pair<std::string, std::vector<std::string>>> response;

    try
    {
        auto reply = bus.call(method);
        reply.read(response);
        if (response.empty())
        {
            log<level::ERR>("Error in mapper response for getting service name",
                            entry("PATH=%s", path.c_str()),
                            entry("INTERFACE=%s", interface.c_str()));
            return std::string{};
        }
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>("Error in mapper method call",
                        entry("ERROR=%s", e.what()),
                        entry("PATH=%s", path.c_str()),
                        entry("INTERFACE=%s", interface.c_str()));
        return std::string{};
    }
    return response[0].first;
}

void mergeFiles(std::vector<std::string>& srcFiles, std::string& dstFile)
{
    std::ofstream outFile(dstFile, std::ios::out);
    for (auto& file : srcFiles)
    {
        std::ifstream inFile;
        inFile.open(file, std::ios_base::in);
        if (!inFile)
        {
            continue;
        }

        inFile.peek();
        if (inFile.eof())
        {
            inFile.close();
            continue;
        }

        outFile << inFile.rdbuf();
        inFile.close();
    }
    outFile.close();
}

namespace internal
{

int executeCmd(const char* path, char** args)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        execv(path, args);

        // execv only retruns on error
        auto error = errno;
        std::string command = path;
        for (int i = 0; args[i]; i++)
        {
            command += " ";
            command += args[i];
        }
        log<level::ERR>("Failed to execute command", entry("ERRNO=%d", error),
                        entry("COMMAND=%s", command.c_str()));
        return -1;
    }
    else if (pid > 0)
    {
        int status;
        if (waitpid(pid, &status, 0) < 0)
        {
            auto error = errno;
            log<level::ERR>("waitpid error", entry("ERRNO=%d", error));
            return -1;
        }
        else if (WEXITSTATUS(status) != 0)
        {
            std::string command = path;
            for (int i = 0; args[i]; i++)
            {
                command += " ";
                command += args[i];
            }
            log<level::ERR>("Error occurred when executing command",
                            entry("STATUS=%d", status),
                            entry("COMMAND=%s", command.c_str()));
            return -1;
        }
    }
    else
    {
        auto error = errno;
        log<level::ERR>("Error occurred during fork", entry("ERRNO=%d", error));
        return -1;
    }

    return 0;
}

} // namespace internal

} // namespace utils
