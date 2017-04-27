#include <string>
#include <experimental/filesystem>
#include <stdlib.h>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
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
    // Need tmp dir to write MANIFEST file to.
    fs::path tmpDirPath(std::string{IMG_UPLOAD_DIR});
    tmpDirPath /= "imageXXXXXX";
    fs::path manifestPath;
    fs::path imageDirPath;

    int status = 0;
    int rc = 0;
    pid_t pid = 0;
    std::string version;
    std::string id;

    auto tmpDir = mkdtemp(const_cast<char*>(tmpDirPath.c_str()));
    if (tmpDir == NULL)
    {
        goto removeDir;
    }
    manifestPath = std::string{tmpDir};
    manifestPath /= MANIFEST_FILE_NAME;
    pid = fork();

    // Get the MANIFEST FILE
    if (pid == 0)
    {
        // child process
        execl("/bin/tar", "tar", "-xf", tarFilePath.c_str(), MANIFEST_FILE_NAME,
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
    version = Version::getValue(manifestPath.string(), "version");
    if (version.empty())
    {
        log<level::ERR>("Error unable to read version from manifest file");
        goto removeDir;
    }

    // Compute id
    id = Version::getId(version);

    fs::remove_all(tmpDir);
    imageDirPath = std::string{IMG_UPLOAD_DIR};
    imageDirPath /= id;

    if (mkdir(imageDirPath.c_str(), S_IRWXU) != 0)
    {
        log<level::ERR>("Error occured during mkdir");
        goto removeTar;
    }

    // Untar tarball
    rc = unTar(tarFilePath, imageDirPath.string());
    if (rc < 0)
    {
        log<level::ERR>("Error occured during untar");
        goto removeTar;
    }
    // Remove tarball
    fs::remove_all(tarFilePath);

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
        execl("/bin/tar", "tar", "-xf", tarFilePath.c_str(),
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
