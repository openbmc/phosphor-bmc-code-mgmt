#include "config.h"

#include "image_manager.hpp"

#include "version.hpp"
#include "watch.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Software/Image/error.hpp>

#include <algorithm>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <random>
#include <string>
#include <system_error>

namespace phosphor
{
namespace software
{
namespace manager
{

PHOSPHOR_LOG2_USING;
using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Software::Image::Error;
namespace Software = phosphor::logging::xyz::openbmc_project::Software;
using ManifestFail = Software::Image::ManifestFileFailure;
using UnTarFail = Software::Image::UnTarFailure;
using InternalFail = Software::Image::InternalFailure;
using ImageFail = Software::Image::ImageFailure;
namespace fs = std::filesystem;

struct RemovablePath
{
    fs::path path;

    explicit RemovablePath(const fs::path& path) : path(path) {}
    ~RemovablePath()
    {
        if (!path.empty())
        {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    }
};

namespace // anonymous
{

std::vector<std::string> getSoftwareObjects(sdbusplus::bus_t& bus)
{
    std::vector<std::string> paths;
    auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                      MAPPER_INTERFACE, "GetSubTreePaths");
    method.append(SOFTWARE_OBJPATH);
    method.append(0); // Depth 0 to search all
    method.append(std::vector<std::string>({VERSION_BUSNAME}));
    auto reply = bus.call(method);
    reply.read(paths);
    return paths;
}

} // namespace

int Manager::processImage(const std::string& tarFilePath)
{
    std::error_code ec;
    if (!fs::is_regular_file(tarFilePath, ec))
    {
        error("Tarball {PATH} does not exist: {ERRNO}", "PATH", tarFilePath,
              "ERRNO", errno);
        report<ManifestFileFailure>(ManifestFail::PATH(tarFilePath.c_str()));
        return -1;
    }
    RemovablePath tarPathRemove(tarFilePath);
    fs::path tmpDirPath(std::string{IMG_UPLOAD_DIR});
    tmpDirPath /= "imageXXXXXX";
    auto tmpDir = tmpDirPath.string();

    // Create a tmp dir to extract tarball.
    if (!mkdtemp(tmpDir.data()))
    {
        error("Error ({ERRNO}) occurred during mkdtemp", "ERRNO", errno);
        report<InternalFailure>(InternalFail::FAIL("mkdtemp"));
        return -1;
    }

    tmpDirPath = tmpDir;
    RemovablePath tmpDirToRemove(tmpDirPath);
    fs::path manifestPath = tmpDirPath;
    manifestPath /= MANIFEST_FILE_NAME;

    // Untar tarball into the tmp dir
    auto rc = unTar(tarFilePath, tmpDirPath.string());
    if (rc < 0)
    {
        error("Error ({RC}) occurred during untar", "RC", rc);
        return -1;
    }

    // Verify the manifest file
    if (!fs::is_regular_file(manifestPath, ec))
    {
        error("No manifest file {PATH}: {ERRNO}", "PATH", tarFilePath, "ERRNO",
              errno);
        report<ManifestFileFailure>(ManifestFail::PATH(tarFilePath.c_str()));
        return -1;
    }

    // Get version
    auto version = Version::getValue(manifestPath.string(), "version");
    if (version.empty())
    {
        error("Unable to read version from manifest file {PATH}", "PATH",
              tarFilePath);
        report<ManifestFileFailure>(ManifestFail::PATH(tarFilePath.c_str()));
        return -1;
    }

    // Get running machine name
    std::string currMachine = Version::getBMCMachine(OS_RELEASE_FILE);
    if (currMachine.empty())
    {
        auto path = OS_RELEASE_FILE;
        error("Failed to read machine name from osRelease: {PATH}", "PATH",
              path);
        report<ImageFailure>(ImageFail::FAIL("Failed to read machine name"),
                             ImageFail::PATH(path));
        return -1;
    }

    // Get machine name for image to be upgraded
    std::string machineStr = Version::getValue(manifestPath.string(),
                                               "MachineName");
    if (!machineStr.empty())
    {
        if (machineStr != currMachine)
        {
            error(
                "BMC upgrade: Machine name doesn't match: {CURRENT_MACHINE} vs {NEW_MACHINE}",
                "CURRENT_MACHINE", currMachine, "NEW_MACHINE", machineStr);
            report<ImageFailure>(
                ImageFail::FAIL("Machine name does not match"),
                ImageFail::PATH(manifestPath.string().c_str()));
            return -1;
        }
    }
    else
    {
        warning("No machine name in Manifest file");
        report<ImageFailure>(
            ImageFail::FAIL("MANIFEST is missing machine name"),
            ImageFail::PATH(manifestPath.string().c_str()));
    }

    // Get purpose
    auto purposeString = Version::getValue(manifestPath.string(), "purpose");
    if (purposeString.empty())
    {
        error("Unable to read purpose from manifest file {PATH}", "PATH",
              tarFilePath);
        report<ManifestFileFailure>(ManifestFail::PATH(tarFilePath.c_str()));
        return -1;
    }

    auto convertedPurpose =
        sdbusplus::message::convert_from_string<Version::VersionPurpose>(
            purposeString);

    if (!convertedPurpose)
    {
        error(
            "Failed to convert manifest purpose ({PURPOSE}) to enum; setting to Unknown.",
            "PURPOSE", purposeString);
    }
    auto purpose = convertedPurpose.value_or(Version::VersionPurpose::Unknown);

    // Get ExtendedVersion
    std::string extendedVersion = Version::getValue(manifestPath.string(),
                                                    "ExtendedVersion");

    // Get CompatibleNames
    std::vector<std::string> compatibleNames =
        Version::getRepeatedValues(manifestPath.string(), "CompatibleName");

    // Compute id
    auto salt = std::to_string(randomGen());
    auto id = Version::getId(version + salt);

    fs::path imageDirPath = std::string{IMG_UPLOAD_DIR};
    imageDirPath /= id;

    auto objPath = std::string{SOFTWARE_OBJPATH} + '/' + id;

    // This service only manages the uploaded versions, and there could be
    // active versions on D-Bus that is not managed by this service.
    // So check D-Bus if there is an existing version.
    auto allSoftwareObjs = getSoftwareObjects(bus);
    auto it = std::find(allSoftwareObjs.begin(), allSoftwareObjs.end(),
                        objPath);
    if (versions.find(id) == versions.end() && it == allSoftwareObjs.end())
    {
        // Rename the temp dir to image dir
        fs::rename(tmpDirPath, imageDirPath, ec);
        // Clear the path, so it does not attemp to remove a non-existing path
        tmpDirToRemove.path.clear();

        // Create Version object
        auto versionPtr = std::make_unique<Version>(
            bus, objPath, version, purpose, extendedVersion,
            imageDirPath.string(), compatibleNames,
            std::bind(&Manager::erase, this, std::placeholders::_1), id);
        versionPtr->deleteObject =
            std::make_unique<phosphor::software::manager::Delete>(bus, objPath,
                                                                  *versionPtr);
        versions.insert(std::make_pair(id, std::move(versionPtr)));
    }
    else
    {
        info("Software Object with the same version ({VERSION}) already exists",
             "VERSION", id);
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

    // Delete image dir
    fs::path imageDirPath = (*(it->second)).path();
    std::error_code ec;
    if (fs::exists(imageDirPath, ec))
    {
        fs::remove_all(imageDirPath, ec);
    }
    this->versions.erase(entryId);
}

int Manager::unTar(const std::string& tarFilePath,
                   const std::string& extractDirPath)
{
    if (tarFilePath.empty())
    {
        error("TarFilePath is empty");
        report<UnTarFailure>(UnTarFail::PATH(tarFilePath.c_str()));
        return -1;
    }
    if (extractDirPath.empty())
    {
        error("ExtractDirPath is empty");
        report<UnTarFailure>(UnTarFail::PATH(extractDirPath.c_str()));
        return -1;
    }

    info("Untaring {PATH} to {EXTRACTIONDIR}", "PATH", tarFilePath,
         "EXTRACTIONDIR", extractDirPath);
    int status = 0;
    pid_t pid = fork();

    if (pid == 0)
    {
        // child process
        execl("/bin/tar", "tar", "-xf", tarFilePath.c_str(), "-C",
              extractDirPath.c_str(), (char*)0);
        // execl only returns on fail
        error("Failed to execute untar on {PATH}", "PATH", tarFilePath);
        report<UnTarFailure>(UnTarFail::PATH(tarFilePath.c_str()));
        return -1;
    }
    else if (pid > 0)
    {
        waitpid(pid, &status, 0);
        if (WEXITSTATUS(status))
        {
            error("Failed ({STATUS}) to untar file {PATH}", "STATUS", status,
                  "PATH", tarFilePath);
            report<UnTarFailure>(UnTarFail::PATH(tarFilePath.c_str()));
            return -1;
        }
    }
    else
    {
        error("fork() failed: {ERRNO}", "ERRNO", errno);
        report<UnTarFailure>(UnTarFail::PATH(tarFilePath.c_str()));
        return -1;
    }

    return 0;
}

} // namespace manager
} // namespace software
} // namespace phosphor
