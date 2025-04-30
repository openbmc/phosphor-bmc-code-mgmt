#include "update_manager.hpp"

#include "device.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/ActivationProgress/server.hpp>
#include <xyz/openbmc_project/Software/ApplyTime/server.hpp>

#include <chrono>
#include <cstring>
#include <stdexcept>

#include <libpldm/firmware_update.h>

PHOSPHOR_LOG2_USING;

namespace phosphor
{
namespace software
{
using namespace std::chrono;
using RequestedApplyTimes = sdbusplus::xyz::openbmc_project::Software::server::
    ApplyTime::RequestedApplyTimes;

UpdateManager::UpdateManager(sdbusplus::async::context& ctxIn) :
    SoftwareManager(ctxIn, "Update.Manager"),
    sdbusplus::server::object_t<
        sdbusplus::xyz::openbmc_project::Software::server::Manager>(
        ctxIn.get_bus(), "/xyz/openbmc_project/software/manager"),
    sdbusplus::aserver::xyz::openbmc_project::software::Update<UpdateManager>(
        ctxIn, "/xyz/openbmc_project/software/manager")
{
    ctxIn.get_bus().request_name("xyz.openbmc_project.Software.Update.Manager");

    // this->force_update(false);
    // this->targets(std::vector<sdbusplus::message::object_path>{});
    // this->update_option(UpdateOptionSupport::StageAndActivate);
}

static auto UpdateManager::method_call(start_update_t /*unused*/,
                                       sdbusplus::message::unix_fd image,
                                       RequestedApplyTimes applyTime)
    -> sdbusplus::async::task<start_update_t::return_type>
{
    info("StartUpdate called with FD={FD}", "FD", image.fd);

    activeSW.clear();

    if (fcntl(image.fd, F_GETFD) == -1)
    {
        error("Invalid file descriptor");
        throw std::runtime_error("Invalid file descriptor for image");
    }

    int newFd = dup(image.fd);
    if (newFd < 0)
    {
        error("Failed to duplicate file descriptor");
        throw std::runtime_error("Failed to duplicate file descriptor");
    }

    UniqueFD safeFd(newFd);

    auto swId = std::to_string(
        duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
    auto objPath = "/xyz/openbmc_project/software/" + swId;

    auto softwarePtr = std::make_shared<phosphor::software::Software>(
        SoftwareManager::ctx, swId);

    softwarePtr->setActivation(sdbusplus::xyz::openbmc_project::Software::
                                   server::Activation::Activations::NotReady);

    if (softwarePtr->softwareActivationProgress)
    {
        softwarePtr->softwareActivationProgress->setProgress(0);
    }

    auto pldmActivationObj =
        co_await doUpdate(softwarePtr, std::move(safeFd), applyTime);

    activeSW[pldmActivationObj] =
        UpdateManager::SWHolder{std::move(softwarePtr)};

    co_await checkDeviceActivationStatus(pldmActivationObj.str);

    co_return sdbusplus::message::object_path(objPath);
}

static sdbusplus::async::task<sdbusplus::message::object_path>
    UpdateManager::doUpdate(std::shared_ptr<phosphor::software::Software> swObj,
                            UniqueFD fd, RequestedApplyTimes applyTime)
{
    auto pldmSoftwareObjPath =
        co_await startPLDMUpdate(std::move(fd), applyTime);
    if (pldmSoftwareObjPath.str.empty())
    {
        swObj->setActivation(sdbusplus::xyz::openbmc_project::Software::server::
                                 Activation::Activations::Failed);
        co_return pldmSoftwareObjPath;
    }
    co_return pldmSoftwareObjPath;
}

static sdbusplus::async::task<sdbusplus::message::object_path>
    UpdateManager::startPLDMUpdate(UniqueFD fd, RequestedApplyTimes applyTime)
{
    if (!fd.is_valid())
    {
        co_return {};
    }

    // Verify the PLDM package checksum before proceeding
    bool checksumValid = co_await verifyPLDMPackageChecksum(fd);
    if (!checksumValid)
    {
        error("PLDM package checksum verification failed, aborting update");
        co_return {};
    }

    int dupFd = dup(fd.get());
    if (dupFd < 0)
    {
        error("Failed to dup FD in startPLDMUpdate: {ERR}", "ERR",
              strerror(errno));
        co_return {};
    }

    UniqueFD localFd(dupFd);

    auto pldmProxy = sdbusplus::async::proxy()
                         .service("xyz.openbmc_project.PLDM")
                         .path("/xyz/openbmc_project/software")
                         .interface("xyz.openbmc_project.Software.Update");

    try
    {
        sdbusplus::message::unix_fd pldmImage(localFd.get());
        auto pldmPath =
            co_await pldmProxy.call<sdbusplus::message::object_path>(
                SoftwareManager::ctx, "StartUpdate",
                sdbusplus::message::unix_fd(pldmImage),
                sdbusplus::common::xyz::openbmc_project::software::
                    convertForMessage(applyTime));

        info("PLDM StartUpdate => {PATH}", "PATH", pldmPath);

        if (pldmPath.str.empty())
        {
            co_return {};
        }

        co_return pldmPath;
    }
    catch (const std::exception& e)
    {
        error("Exception calling PLDM StartUpdate: {ERR}", "ERR", e.what());
        co_return {};
    }
}

static sdbusplus::async::task<void> UpdateManager::checkDeviceActivationStatus(
    const std::string& path)
{
    subscribeToProgressSignal(path);

    try
    {
        auto activationProxy =
            sdbusplus::async::proxy()
                .service("xyz.openbmc_project.PLDM")
                .path(path)
                .interface("xyz.openbmc_project.Software.Activation");

        auto status = co_await activationProxy.get_property<ActivationStatus>(
            SoftwareManager::ctx, "Activation");

        if (status == ActivationStatus::Ready)
        {
            co_await handlePLDMActivation(path, ActivationStatus::Ready);
        }
        subscribeToActivationSignal(path);
    }
    catch (const std::exception& e)
    {
        error("Exception while checking device activation status: {ERR}", "ERR",
              e.what());
        subscribeToActivationSignal(path);
    }
}

static void UpdateManager::subscribeToProgressSignal(const std::string& path)
{
    using namespace sdbusplus::bus::match::rules;

    auto matchRule = propertiesChanged(
        path, "xyz.openbmc_project.Software.ActivationProgress");

    signalMatches.push_back(std::make_unique<sdbusplus::bus::match_t>(
        SoftwareManager::ctx.get_bus(), matchRule,
        [this, path](sdbusplus::message::message& msg) {
            try
            {
                // Properly unpack the PropertiesChanged signal
                std::string interface;
                std::map<std::string, std::variant<uint8_t>> properties;
                std::vector<std::string> invalidated;

                msg.read(interface, properties, invalidated);

                auto it = properties.find("Progress");
                if (it != properties.end())
                {
                    uint8_t progress = std::get<uint8_t>(it->second);
                    info("Received Properties Changed signal. Progress = {P}",
                         "P", progress);
                    this->handlePLDMProgress(path, progress);
                }
            }
            catch (const std::exception& e)
            {
                error("Exception while subscribing to progress signal: {E}",
                      "E", e.what());
            }
        }));
}

static void UpdateManager::subscribeToActivationSignal(const std::string& path)
{
    info("Subscribing to Activation signal: path={P}", "P", path);

    using namespace sdbusplus::bus::match::rules;
    auto matchRule =
        propertiesChanged(path, "xyz.openbmc_project.Software.Activation");

    signalMatches.push_back(std::make_unique<sdbusplus::bus::match_t>(
        SoftwareManager::ctx.get_bus(), matchRule,
        [this, path](sdbusplus::message::message& msg) {
            try
            {
                std::string interface;
                std::map<std::string, std::variant<int>> properties;
                std::vector<std::string> invalidated;

                msg.read(interface, properties, invalidated);

                auto it = properties.find("Activation");
                if (it != properties.end())
                {
                    auto status = static_cast<ActivationStatus>(
                        std::get<int>(it->second));
                    info("Received Activation Changed signal. Status = {S}",
                         "S", static_cast<int>(status));
                    SoftwareManager::ctx.spawn(
                        this->handlePLDMActivation(path, status));
                }
            }
            catch (const std::exception& e)
            {
                error("Exception while subscribing to activation signal: {E}",
                      "E", e.what());
            }
        }));
}

static void UpdateManager::handlePLDMProgress(const std::string& path,
                                              int32_t progress)
{
    auto it = activeSW.find(path);

    if (it == activeSW.end())
    {
        warning("No matching software object path found for {P}", "P",
                std::string(path));
        return;
    }

    auto& swPtr = it->second.sw;
    if (!swPtr->softwareActivationProgress)
    {
        error("No software activation progress object found for {P}", "P",
              std::string(path));
        return;
    }
    swPtr->softwareActivationProgress->setProgress(progress);
}

static sdbusplus::async::task<void> UpdateManager::handlePLDMActivation(
    const std::string& path, ActivationStatus status)
{
    auto it = activeSW.find(path);

    if (it == activeSW.end())
    {
        warning("No matching software object path found for {P}", "P",
                std::string(path));
        co_return;
    }

    auto& swPtr = it->second.sw;

    try
    {
        switch (status)
        {
            case ActivationStatus::Ready:
            {
                info("PLDM software {PATH} is ready for activation", "PATH",
                     std::string(path));

                auto activationProxy =
                    sdbusplus::async::proxy()
                        .service("xyz.openbmc_project.PLDM")
                        .path(path)
                        .interface("xyz.openbmc_project.Software.Activation");
                try
                {
                    co_await activationProxy.set_property(
                        SoftwareManager::ctx, "Activation",
                        sdbusplus::common::xyz::openbmc_project::software::
                            convertForMessage(ActivationStatus::Activating));

                    swPtr->setActivation(
                        sdbusplus::xyz::openbmc_project::Software::server::
                            Activation::Activations::Activating);
                }
                catch (const std::exception& e)
                {
                    error(
                        "Failed to set Activation property to Activating: {ERR}",
                        "ERR", e.what());
                }
                break;
            }

            case ActivationStatus::Active:
            {
                swPtr->setActivation(
                    sdbusplus::xyz::openbmc_project::Software::server::
                        Activation::Activations::Active);
                break;
            }

            case ActivationStatus::Failed:
            {
                error("PLDM software {PATH} activation failed", "PATH",
                      std::string(path));
                swPtr->setActivation(
                    sdbusplus::xyz::openbmc_project::Software::server::
                        Activation::Activations::Failed);
                break;
            }

            default:
                break;
        }
    }
    catch (const std::exception& e)
    {
        error("Exception in handlePLDMActivation: {ERR}", "ERR", e.what());
    }
}

sdbusplus::async::task<bool> UpdateManager::verifyPLDMPackageChecksum(UniqueFD fd)
{
    if (!fd.is_valid())
    {
        error("Invalid file descriptor for PLDM package checksum verification");
        co_return false;
    }

    // Get file size
    struct stat fileStat;
    if (fstat(fd.get(), &fileStat) != 0)
    {
        error("Failed to get file stats for PLDM package: {ERR}", "ERR",
              strerror(errno));
        co_return false;
    }

    // Map the file into memory
    void* mappedData = mmap(nullptr, fileStat.st_size, PROT_READ, MAP_PRIVATE,
                           fd.get(), 0);
    if (mappedData == MAP_FAILED)
    {
        error("Failed to mmap PLDM package file: {ERR}", "ERR",
              strerror(errno));
        co_return false;
    }

    // Verify the payload checksum
    info("Verifying PLDM firmware update package payload checksum...");
    int result = verify_pldm_firmware_update_package_payload_checksum(
        mappedData, fileStat.st_size);

    // Unmap the file
    munmap(mappedData, fileStat.st_size);

    if (result != 0)
    {
        error("PLDM firmware update package payload checksum verification failed: {ERR}",
              "ERR", result);
        co_return false;
    }

    info("PLDM firmware update package payload checksum verification succeeded");
    co_return true;
}

} // namespace software
} // namespace phosphor
