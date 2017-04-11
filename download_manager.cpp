#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <sdbusplus/exception.hpp>
#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>
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
using namespace sdbusplus::xyz::openbmc_project::Common::TFTP::Error;

void Download::downloadViaTFTP(const  std::string fileName,
                               const  std::string serverAddress)
{
    if (fileName.empty())
    {

        elog<DownloadViaTFTP>(
            xyz::openbmc_project::Common::TFTP::DownloadViaTFTP::
            FILENAME(fileName),
            xyz::openbmc_project::Common::TFTP::DownloadViaTFTP::
            SERVERADDRESS(serverAddress));
        log<level::ERR>("Error FileName is empty");
        return;
    }

    if (serverAddress.empty())
    {
        log<level::ERR>("Error ServerAddress is empty");
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
        log<level::ERR>("Error in downloading via TFTP",
                        entry("FILENAME=%s", fileName),
                        entry("SERVERADDRESS=%s", serverAddress));
    }
    else
    {
        log<level::ERR>("Error in fork");
    }

    return;
}

} // namespace manager
} // namespace software
} // namespace phosphor

