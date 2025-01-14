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

PHOSPHOR_LOG2_USING;

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
    std::unique_ptr<Software> softwarePendingIn)
// NOLINTEND(readability-static-accessed-through-instance)
{
    debug("starting the async update with memfd {FD}", "FD", image.fd);

    size_t pldm_pkg_size = 0;

    void* pldm_pkg = pldm_package_util::mmapImagePackage(image, &pldm_pkg_size);

    if (pldm_pkg == NULL)
    {
        softwarePendingIn->setActivation(
            ActivationInterface::Activations::Failed);
        co_return false;
    }

    debug("memory mapped the pldm package successfully");

    std::shared_ptr<PackageParser> packageParser =
        pldm_package_util::parsePLDMPackage(static_cast<uint8_t*>(pldm_pkg),
                                            pldm_pkg_size);

    if (packageParser == nullptr)
    {
        error("could not parse PLDM package");
        softwarePendingIn->setActivation(
            ActivationInterface::Activations::Failed);
        co_return false;
    }

    const bool success = co_await continueUpdateWithMappedPackage(
        pldm_pkg, packageParser, applyTime, softwarePendingIn);

    if (success)
    {
        if (applyTime == RequestedApplyTimes::Immediate)
        {
            currentSoftware = std::move(softwarePendingIn);

            // In case an immediate update is triggered after an update for
            // onReset.
            softwarePending = nullptr;

            info("Successfully updated to software version {SWID}", "SWID",
                 currentSoftware->swid);
        }
        else if (applyTime == RequestedApplyTimes::OnReset)
        {
            softwarePending = std::move(softwarePendingIn);
        }
    }
    else
    {
        softwarePendingIn->setActivation(
            ActivationInterface::Activations::Failed);
        info("Failed to update the software for {SWID}", "SWID",
             currentSoftware->swid);
    }

    if (munmap(pldm_pkg, pldm_pkg_size) != 0)
    {
        error("Failed to un map the PLDM package");
    }

    if (close(image.fd) != 0)
    {
        error("Failed to close file descriptor {FD}", "FD", image.fd);
    }

    co_return success;
}

std::string Device::getEMConfigType() const
{
    return config.configType;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> Device::resetDevice()
// NOLINTEND(readability-static-accessed-through-instance)
{
    info("Default implementation for device reset");

    co_return true;
}

bool Device::setUpdateProgress(uint8_t progress) const
{
    if (!softwarePending || !softwarePending->softwareActivationProgress)
    {
        return false;
    }

    softwarePending->softwareActivationProgress->setProgress(progress);

    return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> Device::continueUpdateWithMappedPackage(
    void* pldm_pkg, const std::shared_ptr<PackageParser>& packageParser,
    sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
        RequestedApplyTimes applyTime,
    const std::unique_ptr<Software>& softwarePendingIn)
// NOLINTEND(readability-static-accessed-through-instance)
{
    int status = 0;

    // extract the component image for the specific device
    size_t componentImageSize = 0;
    uint32_t componentOffset = 0;
    std::string componentVersion;
    status = pldm_package_util::extractMatchingComponentImage(
        packageParser, config.compatibleHardware, config.vendorIANA,
        &componentOffset, &componentImageSize, componentVersion);

    if (status != 0)
    {
        error("could not extract matching component image");

        softwarePendingIn->setActivation(
            ActivationInterface::Activations::Invalid);

        co_return false;
    }

    const uint8_t* matchingComponentImage =
        static_cast<uint8_t*>(pldm_pkg) + componentOffset;

    softwarePendingIn->setActivation(ActivationInterface::Activations::Ready);

    softwarePendingIn->setVersion(componentVersion);

    std::string objPath = softwarePendingIn->objectPath;

    softwarePendingIn->softwareActivationProgress =
        std::make_unique<SoftwareActivationProgress>(ctx, objPath.c_str());

    softwarePendingIn->setActivationBlocksTransition(true);

    softwarePendingIn->setActivation(
        ActivationInterface::Activations::Activating);

    bool success =
        co_await updateDevice(matchingComponentImage, componentImageSize);

    if (success)
    {
        softwarePendingIn->setActivation(
            ActivationInterface::Activations::Active);
    }

    softwarePendingIn->setActivationBlocksTransition(false);

    softwarePendingIn->softwareActivationProgress = nullptr;

    if (!success)
    {
        // do not apply the update, it has failed.
        // We can delete the new software version.

        co_return false;
    }

    if (applyTime == applyTimeImmediate)
    {
        co_await resetDevice();

        co_await softwarePendingIn->createInventoryAssociations(true);

        softwarePendingIn->enableUpdate(allowedApplyTimes);
    }
    else
    {
        co_await softwarePendingIn->createInventoryAssociations(false);
    }

    co_return true;
}
