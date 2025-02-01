#include "update_manager.hpp"

#include "item_updater.hpp"
#include "software_utils.hpp"
#include "version.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Software/Image/error.hpp>

#include <filesystem>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::update
{

namespace fs = std::filesystem;
namespace softwareUtils = phosphor::software::utils;
namespace SoftwareLogging = phosphor::logging::xyz::openbmc_project::software;
namespace SoftwareErrors =
    sdbusplus::error::xyz::openbmc_project::software::image;
using namespace phosphor::logging;
using Version = phosphor::software::manager::Version;
using ActivationIntf = phosphor::software::updater::Activation;
using ManifestFail = SoftwareLogging::image::ManifestFileFailure;
using UnTarFail = SoftwareLogging::image::UnTarFailure;
using InternalFail = SoftwareLogging::image::InternalFailure;
using ImageFail = SoftwareLogging::image::ImageFailure;

void Manager::processImageFailed(sdbusplus::message::unix_fd image,
                                 std::string& id)
{
    close(image);
    updateInProgress = false;
    itemUpdater.updateActivationStatus(id,
                                       ActivationIntf::Activations::Invalid);
}

bool verifyImagePurpose(Version::VersionPurpose purpose,
                        ItemUpdaterIntf::UpdaterType type)
{
    if (purpose == Version::VersionPurpose::Host)
    {
        return (type == ItemUpdaterIntf::UpdaterType::BIOS ||
                type == ItemUpdaterIntf::UpdaterType::ALL);
    }
    return true;
}

// NOLINTNEXTLINE(readability-static-accessed-through-instance)
auto Manager::processImage(sdbusplus::message::unix_fd image,
                           ApplyTimeIntf::RequestedApplyTimes applyTime,
                           std::string id, std::string objPath)
    -> sdbusplus::async::task<>
{
    debug("Processing image {FD}", "FD", image.fd);
    fs::path tmpDirPath(std::string{IMG_UPLOAD_DIR});
    tmpDirPath /= "imageXXXXXX";
    auto tmpDir = tmpDirPath.string();
    // Create a tmp dir to copy tarball.
    if (!mkdtemp(tmpDir.data()))
    {
        error("Error ({ERRNO}) occurred during mkdtemp", "ERRNO", errno);
        processImageFailed(image, id);
        report<SoftwareErrors::InternalFailure>(InternalFail::FAIL("mkdtemp"));
        co_return;
    }

    std::error_code ec;
    tmpDirPath = tmpDir;
    softwareUtils::RemovablePath tmpDirToRemove(tmpDirPath);

    // Untar tarball into the tmp dir
    if (!softwareUtils::unTar(image, tmpDirPath.string()))
    {
        error("Error occurred during untar");
        processImageFailed(image, id);
        report<SoftwareErrors::UnTarFailure>(
            UnTarFail::PATH(tmpDirPath.c_str()));
        co_return;
    }

    fs::path manifestPath = tmpDirPath;
    manifestPath /= MANIFEST_FILE_NAME;

    // Get version
    auto version = Version::getValue(manifestPath.string(), "version");
    if (version.empty())
    {
        error("Unable to read version from manifest file");
        processImageFailed(image, id);
        report<SoftwareErrors::ManifestFileFailure>(
            ManifestFail::PATH(manifestPath.string().c_str()));
        co_return;
    }

    // Get running machine name
    std::string currMachine = Version::getBMCMachine(OS_RELEASE_FILE);
    if (currMachine.empty())
    {
        auto path = OS_RELEASE_FILE;
        error("Failed to read machine name from osRelease: {PATH}", "PATH",
              path);
        processImageFailed(image, id);
        report<SoftwareErrors::ImageFailure>(
            ImageFail::FAIL("Failed to read machine name"),
            ImageFail::PATH(path));
        co_return;
    }

    // Get machine name for image to be upgraded
    std::string machineStr =
        Version::getValue(manifestPath.string(), "MachineName");
    if (!machineStr.empty())
    {
        if (machineStr != currMachine)
        {
            error(
                "BMC upgrade: Machine name doesn't match: {CURRENT_MACHINE} vs {NEW_MACHINE}",
                "CURRENT_MACHINE", currMachine, "NEW_MACHINE", machineStr);
            processImageFailed(image, id);
            report<SoftwareErrors::ImageFailure>(
                ImageFail::FAIL("Machine name does not match"),
                ImageFail::PATH(manifestPath.string().c_str()));
            co_return;
        }
    }
    else
    {
        warning("No machine name in Manifest file");
        report<SoftwareErrors::ImageFailure>(
            ImageFail::FAIL("MANIFEST is missing machine name"),
            ImageFail::PATH(manifestPath.string().c_str()));
    }

    // Get purpose
    auto purposeString = Version::getValue(manifestPath.string(), "purpose");
    if (purposeString.empty())
    {
        error("Unable to read purpose from manifest file");
        processImageFailed(image, id);
        report<SoftwareErrors::ManifestFileFailure>(
            ManifestFail::PATH(manifestPath.string().c_str()));
        co_return;
    }
    auto convertedPurpose =
        sdbusplus::message::convert_from_string<Version::VersionPurpose>(
            purposeString);
    if (!convertedPurpose)
    {
        warning(
            "Failed to convert manifest purpose ({PURPOSE}) to enum; setting to Unknown.",
            "PURPOSE", purposeString);
    }
    auto purpose = convertedPurpose.value_or(Version::VersionPurpose::Unknown);

    if (!verifyImagePurpose(purpose, itemUpdater.type))
    {
        error("Purpose ({PURPOSE}) is not supported", "PURPOSE", purpose);
        processImageFailed(image, id);
        report<SoftwareErrors::ImageFailure>(
            ImageFail::FAIL("Purpose is not supported"),
            ImageFail::PATH(manifestPath.string().c_str()));
        co_return;
    }

    // Get ExtendedVersion
    std::string extendedVersion =
        Version::getValue(manifestPath.string(), "ExtendedVersion");

    // Get CompatibleNames
    std::vector<std::string> compatibleNames =
        Version::getRepeatedValues(manifestPath.string(), "CompatibleName");

    // Rename IMG_UPLOAD_DIR/imageXXXXXX to IMG_UPLOAD_DIR/id as Manifest
    // parsing succedded.
    fs::path imageDirPath = std::string{IMG_UPLOAD_DIR};
    imageDirPath /= id;
    fs::rename(tmpDirPath, imageDirPath, ec);
    tmpDirToRemove.path.clear();

    auto filePath = imageDirPath.string();
    // Create Version object
    auto state = itemUpdater.verifyAndCreateObjects(
        id, objPath, version, purpose, extendedVersion, filePath,
        compatibleNames);
    if (state != ActivationIntf::Activations::Ready)
    {
        error("Software image is invalid");
        processImageFailed(image, id);
        report<SoftwareErrors::ImageFailure>(
            ImageFail::FAIL("Image is invalid"),
            ImageFail::PATH(filePath.c_str()));
        co_return;
    }
    if (applyTime == ApplyTimeIntf::RequestedApplyTimes::Immediate ||
        applyTime == ApplyTimeIntf::RequestedApplyTimes::OnReset)
    {
        itemUpdater.requestActivation(id);
    }

    updateInProgress = false;
    close(image);
    co_return;
}

sdbusplus::message::object_path Manager::startUpdate(
    sdbusplus::message::unix_fd image,
    ApplyTimeIntf::RequestedApplyTimes applyTime)
{
    info("Starting update for image {FD}", "FD", static_cast<int>(image));
    using sdbusplus::xyz::openbmc_project::Common::Error::Unavailable;
    if (updateInProgress)
    {
        error("Failed to start as update is already in progress");
        report<Unavailable>();
        return sdbusplus::message::object_path();
    }
    updateInProgress = true;

    auto id = Version::getId(std::to_string(randomGen()));
    auto objPath = std::string{SOFTWARE_OBJPATH} + '/' + id;

    // Create Activation Object
    itemUpdater.createActivationWithApplyTime(id, objPath, applyTime);

    int newFd = dup(image);
    ctx.spawn(processImage(newFd, applyTime, id, objPath));

    return sdbusplus::message::object_path(objPath);
}

} // namespace phosphor::software::update
