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

using namespace phosphor::software::device;

const auto applyTimeImmediate = sdbusplus::common::xyz::openbmc_project::
    software::ApplyTime::RequestedApplyTimes::Immediate;

const auto ActivationInvalid = ActivationInterface::Activations::Invalid;
const auto ActivationFailed = ActivationInterface::Activations::Failed;

Device::Device(sdbusplus::async::context& ctx, const SoftwareConfig& config,
               manager::SoftwareManager* parent,
               std::set<RequestedApplyTimes> allowedApplyTimes =
                   {RequestedApplyTimes::Immediate,
                    RequestedApplyTimes::OnReset}) :
    allowedApplyTimes(std::move(allowedApplyTimes)), config(config),
    parent(parent), ctx(ctx)
{}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> Device::getImageInfo(
    std::unique_ptr<void, std::function<void(void*)>>& pldmPackage,
    size_t pldmPackageSize, uint8_t** matchingComponentImage,
    size_t* componentImageSize, std::string& componentVersion)

// NOLINTEND(readability-static-accessed-through-instance)
{
    std::shared_ptr<PackageParser> packageParser =
        pldm_package_util::parsePLDMPackage(
            static_cast<uint8_t*>(pldmPackage.get()), pldmPackageSize);

    if (packageParser == nullptr)
    {
        error("could not parse PLDM package");
        co_return false;
    }

    uint32_t componentOffset = 0;
    const int status = pldm_package_util::extractMatchingComponentImage(
        packageParser, config.compatibleHardware, config.vendorIANA,
        &componentOffset, componentImageSize, componentVersion);

    if (status != 0)
    {
        error("could not extract matching component image");
        co_return false;
    }

    *matchingComponentImage =
        static_cast<uint8_t*>(pldmPackage.get()) + componentOffset;

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> Device::startUpdateAsync(
    sdbusplus::message::unix_fd image, RequestedApplyTimes applyTime,
    std::unique_ptr<Software> softwarePendingIn)
// NOLINTEND(readability-static-accessed-through-instance)
{
    debug("starting the async update with memfd {FD}", "FD", image.fd);

    size_t pldm_pkg_size = 0;
    auto pldm_pkg = pldm_package_util::mmapImagePackage(image, &pldm_pkg_size);

    if (pldm_pkg == nullptr)
    {
        softwarePendingIn->setActivation(ActivationInvalid);
        co_return false;
    }

    uint8_t* componentImage;
    size_t componentImageSize = 0;
    std::string componentVersion;

    if (!co_await getImageInfo(pldm_pkg, pldm_pkg_size, &componentImage,
                               &componentImageSize, componentVersion))
    {
        error("could not extract matching component image");
        softwarePendingIn->setActivation(ActivationInvalid);
        co_return false;
    }

    std::unique_ptr<Software> softwarePendingOld = std::move(softwarePending);

    softwarePending = std::move(softwarePendingIn);
    softwarePendingIn = nullptr;

    const bool success = co_await continueUpdateWithMappedPackage(
        componentImage, componentImageSize, componentVersion, applyTime);

    if (!success)
    {
        softwarePending->setActivation(ActivationFailed);
        error("Failed to update the software for {SWID}", "SWID",
              softwareCurrent->swid);

        softwarePending = std::move(softwarePendingOld);

        co_return false;
    }

    if (applyTime == RequestedApplyTimes::Immediate)
    {
        softwareCurrent = std::move(softwarePending);

        // In case an immediate update is triggered after an update for
        // onReset.
        softwarePending = nullptr;

        debug("Successfully updated to software version {SWID}", "SWID",
              softwareCurrent->swid);
    }

    co_return true;
}

std::string Device::getEMConfigType() const
{
    return config.configType;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> Device::resetDevice()
// NOLINTEND(readability-static-accessed-through-instance)
{
    debug("Default implementation for device reset");

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
    const uint8_t* matchingComponentImage, size_t componentImageSize,
    const std::string& componentVersion, RequestedApplyTimes applyTime)
// NOLINTEND(readability-static-accessed-through-instance)
{
    softwarePending->setActivation(ActivationInterface::Activations::Ready);

    softwarePending->setVersion(componentVersion,
                                softwareCurrent->getPurpose());

    std::string objPath = softwarePending->objectPath;

    softwarePending->softwareActivationProgress =
        std::make_unique<SoftwareActivationProgress>(ctx, objPath.c_str());

    softwarePending->setActivationBlocksTransition(true);

    softwarePending->setActivation(
        ActivationInterface::Activations::Activating);

    bool success =
        co_await updateDevice(matchingComponentImage, componentImageSize);

    if (success)
    {
        softwarePending->setActivation(
            ActivationInterface::Activations::Active);
    }

    softwarePending->setActivationBlocksTransition(false);

    softwarePending->softwareActivationProgress = nullptr;

    if (!success)
    {
        // do not apply the update, it has failed.
        // We can delete the new software version.

        co_return false;
    }

    if (applyTime == applyTimeImmediate)
    {
        co_await resetDevice();

        co_await softwarePending->createInventoryAssociations(true);

        softwarePending->enableUpdate(allowedApplyTimes);
    }
    else
    {
        co_await softwarePending->createInventoryAssociations(false);
    }

    co_return true;
}
