#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <phosphor-logging/log.hpp>
#include <experimental/filesystem>
#include <algorithm>
#include "config.h"
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include "xyz/openbmc_project/Common/error.hpp"
#include "download_manager.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;
namespace fs = std::experimental::filesystem;

void Download::downloadViaTFTP(std::string fileName,
                               std::string serverAddress)
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
        log<level::ERR>("Error FileName is empty");
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("FileName"),
                              Argument::ARGUMENT_VALUE(fileName.c_str()));
        return;
    }

    if (serverAddress.empty())
    {
        log<level::ERR>("Error ServerAddress is empty");
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("ServerAddress"),
                              Argument::ARGUMENT_VALUE(serverAddress.c_str()));
        return;
    }

    log<level::INFO>("Downloading via TFTP",
                     entry("FILENAME=%s", fileName),
                     entry("SERVERADDRESS=%s", serverAddress));

    // Check if IMAGE DIR exists and create if necessary.
    fs::path imgDirPath(IMG_UPLOAD_DIR);
    if (!fs::is_directory(imgDirPath))
    {
        fs::create_directory(imgDirPath);
    }

    pid_t pid = fork();

    if (pid == 0)
    {
        // child process
        execl("/usr/bin/tftp", "tftp", "-g", "-r",  fileName.c_str(),
              serverAddress.c_str(), "-l",
              (std::string{IMG_UPLOAD_DIR} + '/' + fileName).c_str(),
              (char*)0);
        // execl only returns on fail
        log<level::ERR>("Error occurred during the TFTP call");
        elog<InternalFailure>();
    }
    else if (pid < 0)
    {
        log<level::ERR>("Error occurred during fork");
        elog<InternalFailure>();
    }

    return;
}

} // namespace manager
} // namespace software
} // namespace phosphor

