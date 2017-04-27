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

struct RemovablePath
{
    fs::path path;

    RemovablePath(fs::path path) : path(path) {}
    ~RemovablePath()
    {
        fs::remove_all(path);
    }
};

int processImage(const std::string& tarFilePath)
{
    if (!fs::is_regular_file(tarFilePath))
    {
        log<level::ERR>("Error tarball does not exist",
                        entry("FILENAME=%s", tarFilePath));
        return -1;

    }
    RemovablePath tarPathRemove(tarFilePath);
    fs::path tmpDirPath(std::string{IMG_UPLOAD_DIR});
    tmpDirPath /= "imageXXXXXX";

    // Need tmp dir to write MANIFEST file to.
    if (!mkdtemp(const_cast<char*>(tmpDirPath.c_str())))
    {
        log<level::ERR>("Error occured during mkdtemp",
                        entry("ERRNO=%d", errno));
        return -1;
    }

    RemovablePath tmpDirRemove(tmpDirPath);
    fs::path manifestPath = tmpDirPath;
    manifestPath /= MANIFEST_FILE_NAME;
    int status = 0;
    pid_t pid = fork();

    // Get the MANIFEST FILE
    if (pid == 0)
    {
        // child process
        execl("/bin/tar", "tar", "-xf", tarFilePath.c_str(), MANIFEST_FILE_NAME,
              "-C", tmpDirPath.c_str(), (char*)0);
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

    // Verify the manifest file
    if (!fs::is_regular_file(manifestPath))
    {
        log<level::ERR>("Error No manifest file");
        return -1;
    }

    // Get version
    auto version = Version::getValue(manifestPath.string(), "version");
    if (version.empty())
    {
        log<level::ERR>("Error unable to read version from manifest file");
        return -1;
    }

    // Compute id
    auto id = Version::getId(version);

    fs::path imageDirPath = std::string{IMG_UPLOAD_DIR};
    imageDirPath /= id;

    if (mkdir(imageDirPath.c_str(), S_IRWXU) != 0)
    {
        log<level::ERR>("Error occured during mkdir",
                        entry("ERRNO=%d", errno));
        return -1;
    }

    // Untar tarball
    auto rc = unTar(tarFilePath, imageDirPath.string());
    if (rc < 0)
    {
        log<level::ERR>("Error occured during untar");
        return -1;
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
    int status = 0;
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
