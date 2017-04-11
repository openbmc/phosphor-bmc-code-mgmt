#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors-CommonTFTP.hpp>
#include "download_manager.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Common::server;
using namespace sdbusplus::xyz::openbmc_project::Common::TFTP::Error;
using namespace phosphor::logging;

void Download::downloadViaTFTP(const  std::string fileName,
                               const  std::string serverAddress)
{
    if (fileName.empty())
    {
        createErrorLog(fileName, serverAddress);
        return;
    }

    if (serverAddress.empty())
    {
        createErrorLog(fileName, serverAddress);
        return;
    }

    log<level::INFO>("Downloading via TFTP",
                     entry("FILENAME=%s", fileName),
                     entry("SERVERADDRESS=%s", serverAddress));

    pid_t pid = fork();

    if (pid > 0)
    {
        // parent process
        int status;
        waitpid(pid, &status, 0);
    }
    else if (pid == 0)
    {
        // child process
        execl("/usr/bin/tftp", "tftp", "-g", "-r",  fileName.c_str(),
              serverAddress.c_str(), "-l", ("/tmp/images/" + fileName).c_str(),
              (char*)0);

        // execl only returns on fail
        createErrorLog(fileName, serverAddress);
    }
    else
    {
        createErrorLog(fileName, serverAddress);
        log<level::ERR>("Error in fork");
    }

    return;
}

void Download::createErrorLog(const std::string& fileName,
                              const std::string& serverAddress)
{
    try
    {
        elog<DownloadViaTFTP>(
            xyz::openbmc_project::Common::TFTP::DownloadViaTFTP::
            FILENAME(fileName),
            xyz::openbmc_project::Common::TFTP::DownloadViaTFTP::
            SERVERADDRESS(serverAddress));
    }
    catch (DownloadViaTFTP& e)
    {
        commit(e.name());
    }

    return;
}

} // namespace manager
} // namespace software
} // namespace phosphor

