#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <phosphor-logging/log.hpp>
#include "download_manager.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Common::server;
using namespace phosphor::logging;

std::string Download::fileName(const std::string value)
{
    // Only call downloadViaTFTP() if both serverAddress and fileName are set
    auto serverAddress = server::TFTP::serverAddress();
    if (serverAddress.empty())
    {
        return server::TFTP::fileName(value);
    }
    downloadViaTFTP(value, serverAddress);

    // Clear fileName and serverAddress
    server::TFTP::serverAddress("");
    return server::TFTP::fileName("");
}

std::string Download::serverAddress(const std::string value)
{
    auto fileName = server::TFTP::fileName();
    if (fileName.empty())
    {
        return server::TFTP::serverAddress(value);
    }
    downloadViaTFTP(fileName, value);

    // Clear fileName and serverAddress
    server::TFTP::fileName("");
    return server::TFTP::serverAddress("");
}

void Download::downloadViaTFTP(const std::string& fileName,
                               const std::string& serverAddress)
{
    log<level::INFO>("Downloading via TFTP",
                     entry("FILENAME=%s", fileName),
                     entry("SERVERADDRESS=%s", serverAddress));

    pid_t pid = fork();
    if (pid == -1)
    {
        log<level::ERR>("Error in fork");
        return;
    }

    if (pid > 0)
    {
        // parent process
        int status;
        waitpid(pid, &status, 0);
    }
    else
    {
        // child process
        execl("/usr/bin/tftp", "tftp", "-g", "-r",  fileName.c_str(),
              serverAddress.c_str(), "-l", ("/tmp/" + fileName).c_str(),
              (char*)0);

        // execl only returns on fail
        log<level::ERR>("Error in downloading via TFTP",
                        entry("FILENAME=%s", fileName),
                        entry("SERVERADDRESS=%s", serverAddress));
    }

    return;
}

} // namespace manager
} // namespace software
} // namespace phosphor

