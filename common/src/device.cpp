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

Device::Device(sdbusplus::async::context& ctx, bool isDryRun,
               const SoftwareConfig& config, SoftwareManager* parent) :
    config(config), parent(parent), dryRun(isDryRun), ctx(ctx)
{}

// NOLINTBEGIN
sdbusplus::async::task<bool> Device::startUpdateAsync(
    sdbusplus::message::unix_fd image, RequestedApplyTimes applyTime,
    std::unique_ptr<Software> softwareUpdate)
// NOLINTEND
{
    lg2::debug("starting the async update with memfd {FD}", "FD", image.fd);

    size_t pldm_pkg_size;

    void* pldm_pkg = pldm_package_util::mmapImagePackage(image, &pldm_pkg_size);

    if (pldm_pkg == NULL)
    {
        co_return false;
    }

    lg2::debug("[Device] mmapped the pldm update package");

    std::shared_ptr<PackageParser> pp = pldm_package_util::parsePLDMPackage(
        static_cast<uint8_t*>(pldm_pkg), pldm_pkg_size);

    if (pp == nullptr)
    {
        lg2::error("could not parse PLDM package");
        co_return false;
    }

    const bool success = co_await continueUpdateWithMappedPackage(
        pldm_pkg, pp, applyTime, softwareUpdate);

    if (success)
    {
        lg2::info("deleting old sw version {SWID}", "SWID",
                  this->softwareCurrent->swid);

        this->softwareCurrent = std::move(softwareUpdate);

        lg2::info("new current sw version: {SWID}", "SWID",
                  this->softwareCurrent->swid);
    }
    else
    {
        lg2::info("update failed, deleting sw update version {SWID}", "SWID",
                  softwareUpdate->swid);
    }

    softwareUpdate = nullptr;

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

void Device::resetDevice()
{
    lg2::info("[Device] default implementation for reset device (nop)");
}

std::set<RequestedApplyTimes> Device::allowedApplyTimes()
{
    return {RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};
}

// NOLINTBEGIN
sdbusplus::async::task<bool> Device::continueUpdateWithMappedPackage(
    void* pldm_pkg, const std::shared_ptr<PackageParser>& packageParser,
    sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
        RequestedApplyTimes applyTime,
    const std::unique_ptr<Software>& softwareUpdate)
// NOLINTEND
{
    int status = 0;

    // extract the component image for the specific device
    size_t matchingComponentImageSize;
    uint32_t matchingComponentOffset;
    status = pldm_package_util::extractMatchingComponentImage(
        packageParser, config.compatibleHardware, config.vendorIANA,
        &matchingComponentOffset, &matchingComponentImageSize);

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

    softwareUpdate->setVersion(packageParser->pkgVersion);

    std::string objPath = softwareUpdate->getObjectPath();

    softwareUpdate->optSoftwareActivationProgress =
        std::make_unique<SoftwareActivationProgress>(ctx, objPath.c_str());

    softwareUpdate->setActivationBlocksTransition(true);

    softwareUpdate->setActivation(ActivationInterface::Activations::Activating);

    bool success = co_await updateDevice(
        matchingComponentImage, matchingComponentImageSize,
        softwareUpdate->optSoftwareActivationProgress);

    if (success)
    {
        softwareUpdate->setActivation(ActivationInterface::Activations::Active);
    }

    softwareUpdate->setActivationBlocksTransition(false);

    softwareUpdate->optSoftwareActivationProgress = nullptr;

    if (!success)
    {
        // do not apply the update, it has failed.
        // We can delete the new software version.

        co_return false;
    }

    if (applyTime == applyTimeImmediate)
    {
        this->resetDevice();

        co_await softwareUpdate->setAssociationDefinitionsRunningActivating(
            true, false);

        softwareUpdate->enableUpdate(this->allowedApplyTimes());
    }
    else
    {
        co_await softwareUpdate->setAssociationDefinitionsRunningActivating(
            false, true);
    }

    co_return true;
}
