#include "config.h"

#include "image_manager.hpp"

#include "version.hpp"
#include "watch.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <elog-errors.hpp>
#include <experimental/filesystem>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <string>
#include <xyz/openbmc_project/Software/Image/error.hpp>

namespace phosphor
{
namespace software
{
namespace manager
{

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Software::Image::Error;
namespace Software = phosphor::logging::xyz::openbmc_project::Software;
using ManifestFail = Software::Image::ManifestFileFailure;
using UnTarFail = Software::Image::UnTarFailure;
using InternalFail = Software::Image::InternalFailure;
namespace fs = std::experimental::filesystem;

struct RemovablePath
{
    fs::path path;

    RemovablePath(const fs::path& path) : path(path)
    {
    }
    ~RemovablePath()
    {
        if (fs::exists(path))
        {
            fs::remove_all(path);
        }
        else
        {
            // Path should exist
            log<level::ERR>("Error removable path does not exist",
                            entry("PATH=%s", path.c_str()));
        }
    }
};

int Manager::processImage(const std::string& tarFilePath)
{
    if (!fs::is_regular_file(tarFilePath))
    {
        log<level::ERR>("Error tarball does not exist",
                        entry("FILENAME=%s", tarFilePath.c_str()));
        report<ManifestFileFailure>(ManifestFail::PATH(tarFilePath.c_str()));
        return -1;
    }
    RemovablePath tarPathRemove(tarFilePath);
    fs::path tmpDirPath(std::string{IMG_UPLOAD_DIR});
    tmpDirPath /= "imageXXXXXX";

    // Need tmp dir to write MANIFEST file to.
    if (!mkdtemp(const_cast<char*>(tmpDirPath.c_str())))
    {
        log<level::ERR>("Error occurred during mkdtemp",
                        entry("ERRNO=%d", errno));
        report<InternalFailure>(InternalFail::FAIL("mkdtemp"));
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
        log<level::ERR>("Failed to execute extract manifest",
                        entry("FILENAME=%s", tarFilePath.c_str()));
        report<ManifestFileFailure>(ManifestFail::PATH(tarFilePath.c_str()));
        return -1;
    }
    else if (pid > 0)
    {
        waitpid(pid, &status, 0);
        if (WEXITSTATUS(status))
        {
            log<level::ERR>("Failed to extract manifest",
                            entry("FILENAME=%s", tarFilePath.c_str()));
            report<UnTarFailure>(UnTarFail::PATH(tarFilePath.c_str()));
            return -1;
        }
    }
    else
    {
        log<level::ERR>("fork() failed.");
        report<InternalFailure>(InternalFail::FAIL("fork"));
        return -1;
    }

    // Verify the manifest file
    if (!fs::is_regular_file(manifestPath))
    {
        log<level::ERR>("Error No manifest file",
                        entry("FILENAME=%s", tarFilePath.c_str()));
        report<ManifestFileFailure>(ManifestFail::PATH(tarFilePath.c_str()));
        return -1;
    }

    // Get version
    auto version = Version::getValue(manifestPath.string(), "version");
    if (version.empty())
    {
        log<level::ERR>("Error unable to read version from manifest file");
        report<ManifestFileFailure>(ManifestFail::PATH(tarFilePath.c_str()));
        return -1;
    }

    // Get purpose
    auto purposeString = Version::getValue(manifestPath.string(), "purpose");
    if (purposeString.empty())
    {
        log<level::ERR>("Error unable to read purpose from manifest file");
        report<ManifestFileFailure>(ManifestFail::PATH(tarFilePath.c_str()));
        return -1;
    }

    auto purpose = Version::VersionPurpose::Unknown;
    try
    {
        purpose = Version::convertVersionPurposeFromString(purposeString);
    }
    catch (const sdbusplus::exception::InvalidEnumString& e)
    {
        log<level::ERR>("Error: Failed to convert manifest purpose to enum."
                        " Setting to Unknown.");
    }

    // Compute id
    auto id = Version::getId(version);

    fs::path imageDirPath = std::string{IMG_UPLOAD_DIR};
    imageDirPath /= id;

    if (fs::exists(imageDirPath))
    {
        fs::remove_all(imageDirPath);
    }
    if (mkdir(imageDirPath.c_str(), S_IRWXU) != 0)
    {
        log<level::ERR>("Error occurred during mkdir",
                        entry("ERRNO=%d", errno));
        report<InternalFailure>(InternalFail::FAIL("mkdir"));
        return -1;
    }

    // Untar tarball
    auto rc = unTar(tarFilePath, imageDirPath.string());
    if (rc < 0)
    {
        log<level::ERR>("Error occurred during untar");
        return -1;
    }

    // Create Version object
    auto objPath = std::string{SOFTWARE_OBJPATH} + '/' + id;

    if (versions.find(id) == versions.end())
    {
        auto versionPtr = std::make_unique<Version>(
            bus, objPath, version, purpose, imageDirPath.string(),
            std::bind(&Manager::erase, this, std::placeholders::_1));
        versionPtr->deleteObject =
            std::make_unique<phosphor::software::manager::Delete>(bus, objPath,
                                                                  *versionPtr);
        versions.insert(std::make_pair(id, std::move(versionPtr)));
    }
    else
    {
        log<level::INFO>("Software Object with the same version already exists",
                         entry("VERSION_ID=%s", id.c_str()));
    }
    return 0;
}

void Manager::erase(std::string entryId)
{
    auto it = versions.find(entryId);
    if (it == versions.end())
    {
        return;
    }

    if (it->second->isFunctional())
    {
        log<level::ERR>(("Error: Version " + entryId +
                         " is currently running on the BMC."
                         " Unable to remove.")
                            .c_str());
        return;
    }

    // Delete image dir
    fs::path imageDirPath = (*(it->second)).path();
    if (fs::exists(imageDirPath))
    {
        fs::remove_all(imageDirPath);
    }
    this->versions.erase(entryId);
}

int Manager::unTar(const std::string& tarFilePath,
                   const std::string& extractDirPath)
{
    if (tarFilePath.empty())
    {
        log<level::ERR>("Error TarFilePath is empty");
        report<UnTarFailure>(UnTarFail::PATH(tarFilePath.c_str()));
        return -1;
    }
    if (extractDirPath.empty())
    {
        log<level::ERR>("Error ExtractDirPath is empty");
        report<UnTarFailure>(UnTarFail::PATH(extractDirPath.c_str()));
        return -1;
    }

    log<level::INFO>("Untaring", entry("FILENAME=%s", tarFilePath.c_str()),
                     entry("EXTRACTIONDIR=%s", extractDirPath.c_str()));
    int status = 0;
    pid_t pid = fork();

    if (pid == 0)
    {
        // child process
        execl("/bin/tar", "tar", "-xf", tarFilePath.c_str(), "-C",
              extractDirPath.c_str(), (char*)0);
        // execl only returns on fail
        log<level::ERR>("Failed to execute untar file",
                        entry("FILENAME=%s", tarFilePath.c_str()));
        report<UnTarFailure>(UnTarFail::PATH(tarFilePath.c_str()));
        return -1;
    }
    else if (pid > 0)
    {
        waitpid(pid, &status, 0);
        if (WEXITSTATUS(status))
        {
            log<level::ERR>("Failed to untar file",
                            entry("FILENAME=%s", tarFilePath.c_str()));
            report<UnTarFailure>(UnTarFail::PATH(tarFilePath.c_str()));
            return -1;
        }
    }
    else
    {
        log<level::ERR>("fork() failed.");
        report<UnTarFailure>(UnTarFail::PATH(tarFilePath.c_str()));
        return -1;
    }

    return 0;
}

} // namespace manager
} // namespace software
} // namespace phosphor
