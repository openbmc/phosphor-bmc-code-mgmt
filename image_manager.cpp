#include <string>
#include <experimental/filesystem>
#include <stdlib.h>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
#include <algorithm>
#include <sys/wait.h>
#include <phosphor-logging/log.hpp>
#include "config.h"
#include "version.hpp"
#include "image_manager.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using namespace phosphor::logging;
namespace fs = std::experimental::filesystem;

int processImage(const std::string& tarFilePath)
{
    // Need tmp dir since id is not calculated yet.
    char dir[50] = IMG_UPLOAD_DIR;
    strcat(dir, "/imageXXXXXX");
    auto tmpDir = mkdtemp(dir);

    // Untar tarball
    auto rc = unTar(tarFilePath, tmpDir);
    // Remove tarball
    fs::remove_all(tarFilePath);
    if (rc < 0)
    {
        log<level::ERR>("Error occured during untar");
        fs::remove_all(tmpDir);
        return -1;
    }

    // Verify the manifest file
    auto manifestFile = std::string{tmpDir} + "/MANIFEST";
    fs::path manifestPath(manifestFile);
    if (!fs::is_regular_file(manifestPath))
    {
        log<level::ERR>("Error No manifest file");
        fs::remove_all(tmpDir);
        return -1;
    }

    // Get version
    auto version = Version::getValue(manifestFile, "version");
    if (version.empty())
    {
        log<level::ERR>("Error unable to read version from manifest file");
        fs::remove_all(tmpDir);
        return -1;
    }

    // Get purpose
    auto purposeString = Version::getValue(manifestFile, "purpose");
    if (purposeString.empty())
    {
        log<level::ERR>("Error unable to read purpose from manifest file");
        fs::remove_all(tmpDir);
        return -1;
    }

    std::transform(purposeString.begin(), purposeString.end(),
                   purposeString.begin(), ::tolower);

    auto purpose = Version::VersionPurpose::Unknown;
    if (purposeString.compare("bmc"))
    {
        purpose = Version::VersionPurpose::BMC;
    }
    else if (purposeString.compare("host"))
    {
        purpose = Version::VersionPurpose::Host;
    }
    else if (purposeString.compare("system"))
    {
        purpose = Version::VersionPurpose::System;
    }
    else if (purposeString.compare("other"))
    {
        purpose = Version::VersionPurpose::Other;
    }

    // Compute id
    auto id = Version::getId(version);

    // Copy tmp dir to image dir
    auto imageDir = std::string{IMG_UPLOAD_DIR} + '/' + id;
    fs::copy(tmpDir, imageDir);
    fs::remove_all(tmpDir);

    // Create Version object
    auto bus = sdbusplus::bus::new_default();
    phosphor::software::manager::Version manager(bus,
            std::string{SOFTWARE_OBJPATH} + '/' + id, version,
            purpose, imageDir);

    bus.request_name(VERSION_BUSNAME);

    while (true)
    {
        bus.process_discard();
        bus.wait();
    }

    return 0;
}

int unTar(const std::string& tarFilePath, const std::string& extractDirPath)
{
    if (tarFilePath.empty())
    {
        log<level::ERR>("Error TarFilePath is empty");
        return -1;
    }
    if (extractDirPath.empty())
    {
        log<level::ERR>("Error ExtractDirPath is empty");
        return -1;
    }

    log<level::INFO>("Untaring",
                     entry("FILENAME=%s", tarFilePath),
                     entry("EXTRACTIONDIR=%s", extractDirPath));
    int status;
    pid_t pid = fork();

    if (pid == 0)
    {
        // child process
        execl("/bin/tar", "tar", "-xvf", tarFilePath.c_str(),
              "-C", extractDirPath.c_str(), (char*)0);
        // execl only returns on fail
        log<level::ERR>("Failed to untar file",
                        entry("FILENAME=%s", tarFilePath));
        return -1;
    }
    else if (pid > 0)
    {
        waitpid(pid, &status, 0);
    }
    else
    {
        log<level::ERR>("fork() failed.");
        return -1;
    }

    return 0;
}
} // namespace manager
} // namespace software
} // namepsace phosphor
