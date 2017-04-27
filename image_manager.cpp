#include <string>
#include <experimental/filesystem>
#include <stdlib.h>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
#include <algorithm>
#include <sys/wait.h>
#include <sys/stat.h>
#include <phosphor-logging/log.hpp>
#include "config.h"
#include "version.hpp"
#include "watch.hpp"
#include "image_manager.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using namespace phosphor::logging;
namespace fs = std::experimental::filesystem;

int processImage(const std::string& tarFilePath, void* userdata)
{
    // Need tmp dir to write MANIFEST file to.
    char dir[50] = IMG_UPLOAD_DIR;
    strcat(dir, "/imageXXXXXX");
    auto tmpDir = mkdtemp(dir);
    auto manifestFile = std::string{tmpDir} + "/MANIFEST";
    fs::path manifestPath(manifestFile);
    auto* watch = static_cast<Watch*>(userdata);

    int status;
    int rc;
    std::string imageDir;
    std::string version;
    std::string id;
    std::string objPath;
    std::string purposeString;
    auto purpose = Version::VersionPurpose::Unknown;

    pid_t pid = fork();

    // Get the MANIFEST FILE
    if (pid == 0)
    {
        // child process
        execl("/bin/tar", "tar", "-xf", tarFilePath.c_str(), "MANIFEST",
              "-C", tmpDir, (char*)0);
        // execl only returns on fail
        log<level::ERR>("Failed to untar file",
                        entry("FILENAME=%s", tarFilePath));
        goto removeDir;
    }
    else if (pid > 0)
    {
        waitpid(pid, &status, 0);
    }
    else
    {
        log<level::ERR>("fork() failed.");
        goto removeDir;
    }

    // Verify the manifest file
    if (!fs::is_regular_file(manifestPath))
    {
        log<level::ERR>("Error No manifest file");
        goto removeDir;
    }

    // Get version
    version = Version::getValue(manifestFile, "version");
    if (version.empty())
    {
        log<level::ERR>("Error unable to read version from manifest file");
        goto removeDir;
    }

    // Get purpose
    purposeString = Version::getValue(manifestFile, "purpose");
    if (purposeString.empty())
    {
        log<level::ERR>("Error unable to read purpose from manifest file");
        goto removeDir;
    }

    std::transform(purposeString.begin(), purposeString.end(),
                   purposeString.begin(), ::tolower);

    if (purposeString.compare("bmc") == 0)
    {
        purpose = Version::VersionPurpose::BMC;
    }
    else if (purposeString.compare("host") == 0)
    {
        purpose = Version::VersionPurpose::Host;
    }
    else if (purposeString.compare("system") == 0)
    {
        purpose = Version::VersionPurpose::System;
    }
    else if (purposeString.compare("other") == 0)
    {
        purpose = Version::VersionPurpose::Other;
    }

    // Compute id
    id = Version::getId(version);

    fs::remove_all(tmpDir);
    imageDir = std::string{IMG_UPLOAD_DIR} + '/' + id;

    mkdir(imageDir.c_str(), S_IRWXU);

    // Untar tarball
    rc = unTar(tarFilePath, imageDir);
    if (rc < 0)
    {
        log<level::ERR>("Error occured during untar");
        goto removeTar;
    }

    // Remove tarball
    fs::remove_all(tarFilePath);

    // Create Version object
    objPath =  std::string{SOFTWARE_OBJPATH} + '/' + id;
    watch->versions.insert(std::make_pair(
                               id,
                               std::make_unique<Version>(
                                   watch->bus,
                                   objPath,
                                   version,
                                   purpose,
                                   imageDir)));

    return 0;

removeDir:

    fs::remove_all(tmpDir);

removeTar:

    fs::remove_all(tarFilePath);
    return -1;
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
