#include "device.hpp"

#include "common/pldm/pldm_package_util.hpp"
#include "software.hpp"
#include "software_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

#include <utility>

const auto applyTimeImmediate = sdbusplus::common::xyz::openbmc_project::
    software::ApplyTime::RequestedApplyTimes::Immediate;

Device::Device(sdbusplus::async::context& ctx, const SoftwareConfig& config,
               SoftwareManager* parent,
               std::set<RequestedApplyTimes> allowedApplyTimes =
                   {RequestedApplyTimes::Immediate,
                    RequestedApplyTimes::OnReset}) :
    allowedApplyTimes(std::move(allowedApplyTimes)), config(config),
    parent(parent), ctx(ctx)
{}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> Device::startUpdateAsync(
    sdbusplus::message::unix_fd image, RequestedApplyTimes applyTime,
    std::unique_ptr<Software> softwareUpdateExternal)
// NOLINTEND(readability-static-accessed-through-instance)
{
    lg2::debug("starting the async update with memfd {FD}", "FD", image.fd);

    size_t pldm_pkg_size = 0;

    void* pldm_pkg = pldm_package_util::mmapImagePackage(image, &pldm_pkg_size);

    if (pldm_pkg == NULL)
    {
        softwareUpdateExternal->setActivation(
            ActivationInterface::Activations::Failed);
        co_return false;
    }

    lg2::debug("[Device] mmapped the pldm update package");

    std::shared_ptr<PackageParser> pp = pldm_package_util::parsePLDMPackage(
        static_cast<uint8_t*>(pldm_pkg), pldm_pkg_size);

    if (pp == nullptr)
    {
        lg2::error("could not parse PLDM package");
        softwareUpdateExternal->setActivation(
            ActivationInterface::Activations::Failed);
        co_return false;
    }

    const bool success = co_await continueUpdateWithMappedPackage(
        pldm_pkg, pp, applyTime, softwareUpdateExternal);

    if (success)
    {
        if (applyTime == RequestedApplyTimes::Immediate)
        {
            lg2::info("deleting old sw version {SWID}", "SWID",
                      this->softwareCurrent->swid);

            this->softwareCurrent = std::move(softwareUpdateExternal);

            // In case an immediate update is triggered after an update for
            // onReset.
            this->pendingSoftwareVersion = nullptr;
        }
        else if (applyTime == RequestedApplyTimes::OnReset)
        {
            this->pendingSoftwareVersion = std::move(softwareUpdateExternal);
        }
        else
        {
            softwareUpdateExternal->setActivation(
                ActivationInterface::Activations::Failed);
            lg2::error("unhandled apply time {APPLYTIME}", "APPLYTIME",
                       applyTime);
        }

        lg2::info("new current sw version: {SWID}", "SWID",
                  this->softwareCurrent->swid);
    }
    else
    {
        softwareUpdateExternal->setActivation(
            ActivationInterface::Activations::Failed);
        lg2::info("update failed, deleting sw update version {SWID}", "SWID",
                  softwareUpdateExternal->swid);
    }

    softwareUpdateExternal = nullptr;

    if (munmap(pldm_pkg, pldm_pkg_size) != 0)
    {
        lg2::error("[Device] failed to munmap the pldm package");
    }

    if (close(image.fd) != 0)
    {
        lg2::error("[Device] failed to close file descriptor {FD}", "FD",
                   image.fd);
    }

    co_return success;
}

std::string Device::getEMConfigType() const
{
    return this->config.configType;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> Device::resetDevice()
// NOLINTEND(readability-static-accessed-through-instance)
{
    lg2::info("Default implementation for device reset");

    co_return true;
}

void Device::setUpdateProgress(uint8_t progress) const
{
    if (this->pendingSoftwareVersion == nullptr)
    {
        return;
    }

    const auto& optProgress =
        this->pendingSoftwareVersion->softwareActivationProgress;

    if (optProgress == nullptr)
    {
        return;
    }

    optProgress->setProgress(progress);
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> Device::continueUpdateWithMappedPackage(
    void* pldm_pkg, const std::shared_ptr<PackageParser>& packageParser,
    sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
        RequestedApplyTimes applyTime,
    const std::unique_ptr<Software>& softwareUpdate)
// NOLINTEND(readability-static-accessed-through-instance)
{
    int status = 0;

    // extract the component image for the specific device
    size_t matchingComponentImageSize;
    uint32_t matchingComponentOffset;
    std::string matchingComponentVersion;
    status = pldm_package_util::extractMatchingComponentImage(
        packageParser, config.compatibleHardware, config.vendorIANA,
        &matchingComponentOffset, &matchingComponentImageSize,
        matchingComponentVersion);

    if (status != 0)
    {
        lg2::error("could not extract matching component image");

        softwareUpdate->setActivation(
            ActivationInterface::Activations::Invalid);

        co_return false;
    }

    const uint8_t* matchingComponentImage =
        static_cast<uint8_t*>(pldm_pkg) + matchingComponentOffset;

    softwareUpdate->setActivation(ActivationInterface::Activations::Ready);

    softwareUpdate->setVersion(matchingComponentVersion);

    std::string objPath = softwareUpdate->getObjectPath();

    softwareUpdate->softwareActivationProgress =
        std::make_unique<SoftwareActivationProgress>(ctx, objPath.c_str());

    softwareUpdate->setActivationBlocksTransition(true);

    softwareUpdate->setActivation(ActivationInterface::Activations::Activating);

    bool success = co_await updateDevice(matchingComponentImage,
                                         matchingComponentImageSize);

    if (success)
    {
        softwareUpdate->setActivation(ActivationInterface::Activations::Active);
    }

    softwareUpdate->setActivationBlocksTransition(false);

    softwareUpdate->softwareActivationProgress = nullptr;

    if (!success)
    {
        // do not apply the update, it has failed.
        // We can delete the new software version.

        co_return false;
    }

    if (applyTime == applyTimeImmediate)
    {
        co_await this->resetDevice();

        co_await softwareUpdate->createInventoryAssociations(true, false);

        softwareUpdate->enableUpdate(this->allowedApplyTimes);
    }
    else
    {
        co_await softwareUpdate->createInventoryAssociations(false, true);
    }

    co_return true;
}
