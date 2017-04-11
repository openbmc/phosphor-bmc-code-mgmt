#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <phosphor-logging/log.hpp>
#include "config.h"
#include "download_manager.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using namespace phosphor::logging;

void Download::downloadViaTFTP(const  std::string fileName,
                               const  std::string serverAddress)
{
    if (fileName.empty())
    {
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
        return;
    }

    if (serverAddress.empty())
    {
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
        return;
    }

    log<level::INFO>("Downloading via TFTP",
                     entry("FILENAME=%s", fileName),
                     entry("SERVERADDRESS=%s", serverAddress));

    pid_t pid = fork();

    if (pid == 0)
    {
        // child process
        execl("/usr/bin/tftp", "tftp", "-g", "-r",  fileName.c_str(),
              serverAddress.c_str(), "-l", (IMAGE_DIR + fileName).c_str(),
              (char*)0);
        // execl only returns on fail
        sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure();
    }
    else if (pid < 0)
    {
        sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure();
    }

    return;
}

} // namespace manager
} // namespace software
} // namespace phosphor

