#include "config.h"

#include "download_manager.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

namespace phosphor
{
namespace software
{
namespace manager
{

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
PHOSPHOR_LOG2_USING;
using namespace phosphor::logging;
namespace fs = std::filesystem;

void Download::downloadViaTFTP(std::string fileName, std::string serverAddress)
{
    using Argument = xyz::openbmc_project::Common::InvalidArgument;

    // Sanitize the fileName string
    if (!fileName.empty())
    {
        fileName.erase(std::remove(fileName.begin(), fileName.end(), '/'),
                       fileName.end());
        fileName = fileName.substr(fileName.find_first_not_of('.'));
    }

    if (fileName.empty())
    {
        error("Filename is empty");
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("FileName"),
                              Argument::ARGUMENT_VALUE(fileName.c_str()));
        return;
    }

    if (serverAddress.empty())
    {
        error("ServerAddress is empty");
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("ServerAddress"),
                              Argument::ARGUMENT_VALUE(serverAddress.c_str()));
        return;
    }

    info("Downloading {PATH} via TFTP: {SERVERADDRESS}", "PATH", fileName,
         "SERVERADDRESS", serverAddress);

    // Check if IMAGE DIR exists
    fs::path imgDirPath(IMG_UPLOAD_DIR);
    std::error_code ec;
    if (!fs::is_directory(imgDirPath, ec))
    {
        error("Image Dir {PATH} does not exist: {ERRNO}", "PATH", imgDirPath,
              "ERRNO", errno);
        elog<InternalFailure>();
        return;
    }

    pid_t pid = fork();

    if (pid == 0)
    {
        pid_t nextPid = fork();
        if (nextPid == 0)
        {
            // child process
            execl("/usr/bin/tftp", "tftp", "-g", "-r", fileName.c_str(),
                  serverAddress.c_str(), "-l",
                  (std::string{IMG_UPLOAD_DIR} + '/' + fileName).c_str(),
                  (char*)0);
            // execl only returns on fail
            error("Error ({ERRNO}) occurred during the TFTP call", "ERRNO",
                  errno);
            elog<InternalFailure>();
        }
        else if (nextPid < 0)
        {
            error("Error ({ERRNO}) occurred during fork", "ERRNO", errno);
            elog<InternalFailure>();
        }
        // do nothing as parent if all is going well
        // when parent exits, child will be reparented under init
        // and then be reaped properly
        exit(0);
    }
    else if (pid < 0)
    {
        error("Error ({ERRNO}) occurred during fork", "ERRNO", errno);
        elog<InternalFailure>();
    }
    else
    {
        int status;
        if (waitpid(pid, &status, 0) < 0)
        {
            error("Error ({ERRNO}) occurred during waitpid", "ERRNO", errno);
        }
        else if (WEXITSTATUS(status) != 0)
        {
            error("Failed ({STATUS}) to launch tftp", "STATUS", status);
        }
    }

    return;
}

} // namespace manager
} // namespace software
} // namespace phosphor
