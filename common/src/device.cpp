#include "device.hpp"

#include "common/pldm/pldm_package_util.hpp"
#include "software.hpp"
#include "software_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/ActivationProgress/aserver.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

#include <utility>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software::device;

using SoftwareActivationProgress =
    sdbusplus::aserver::xyz::openbmc_project::software::ActivationProgress<
        phosphor::software::Software>;

using SoftwareActivationProgressProperties = sdbusplus::common::xyz::
    openbmc_project::software::ActivationProgress::properties_t;

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
    parent(parent), ctx(ctx), events(ctx)
{}

sdbusplus::async::task<bool> Device::getImageInfo(
    const sdbusplus::object_path& objectPath,
    std::unique_ptr<void, std::function<void(void*)>>& pldmPackage,
    size_t pldmPackageSize, std::vector<ComponentImage>& components,
    std::string& componentVersion)

{
    std::unique_ptr<pldm::fw_update::Package> package =
        pldm_package_util::parsePLDMPackage(
            static_cast<uint8_t*>(pldmPackage.get()), pldmPackageSize);

    if (package == nullptr)
    {
        error("could not parse PLDM package");
        co_await events.generateVerificationFailed(objectPath, componentVersion,
                                                   true);
        co_return false;
    }

    co_await events.generateVerificationFailed(objectPath, componentVersion,
                                               false);

    std::vector<pldm_package_util::MatchingComponentImage> matching;
    const int status = pldm_package_util::extractMatchingComponentImages(
        static_cast<uint8_t*>(pldmPackage.get()), package,
        config.compatibleHardware, config.vendorIANA, matching);

    if (status != 0)
    {
        error("could not extract matching component images");
        co_await events.generateUpdateNotApplicable(objectPath,
                                                    componentVersion, true);
        co_return false;
    }

    components.clear();
    for (const auto& m : matching)
    {
        components.emplace_back(ComponentImage{
            static_cast<uint8_t*>(pldmPackage.get()) + m.offset, m.size,
            m.version});
    }

    componentVersion = components.front().version;

    co_await events.generateUpdateNotApplicable(objectPath, componentVersion,
                                                false);

    co_return true;
}

sdbusplus::async::task<bool> Device::startUpdateAsync(
    sdbusplus::message::unix_fd image, RequestedApplyTimes applyTime,
    std::unique_ptr<Software> softwarePendingIn)
{
    debug("starting the async update with memfd {FD}", "FD", image.fd);

    size_t pldm_pkg_size = 0;
    auto pldm_pkg = pldm_package_util::mmapImagePackage(image, &pldm_pkg_size);

    if (pldm_pkg == nullptr)
    {
        softwarePendingIn->setActivation(ActivationInvalid);
        co_return false;
    }

    std::vector<ComponentImage> components;
    std::string componentVersion;

    if (!co_await getImageInfo(softwarePendingIn->objectPath, pldm_pkg,
                               pldm_pkg_size, components, componentVersion))
    {
        softwarePendingIn->setActivation(ActivationInvalid);
        co_return false;
    }

    std::unique_ptr<Software> softwarePendingOld = std::move(softwarePending);

    softwarePending = std::move(softwarePendingIn);
    softwarePendingIn = nullptr;

    co_await events.generateTargetDetermined(softwarePending->objectPath,
                                             componentVersion);

    const bool success = co_await continueUpdateWithMappedPackage(
        components, componentVersion, applyTime);

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

sdbusplus::async::task<bool> Device::resetDevice()
{
    debug("Default implementation for device reset");

    co_return true;
}

sdbusplus::async::task<bool> Device::updateDeviceComponents(
    const std::vector<ComponentImage>& components)
{
    // Single-component devices consume only the first applicable
    // component image, which preserves the previous behavior.
    co_return co_await updateDevice(components.front().image,
                                    components.front().size);
}

bool Device::setUpdateProgress(uint8_t progress) const
{
    if (!softwarePending || !softwarePending->softwareActivationProgress)
    {
        return false;
    }

    softwarePending->softwareActivationProgress->progress(progress);

    return true;
}

sdbusplus::async::task<bool> Device::continueUpdateWithMappedPackage(
    const std::vector<ComponentImage>& components,
    const std::string& componentVersion, RequestedApplyTimes applyTime)
{
    softwarePending->setActivation(ActivationInterface::Activations::Ready);

    softwarePending->setVersion(componentVersion,
                                softwareCurrent->getPurpose().value_or(
                                    SoftwareVersion::VersionPurpose::Unknown));

    std::string objPath = softwarePending->objectPath;

    softwarePending->softwareActivationProgress =
        std::make_unique<SoftwareActivationProgress>(
            ctx, objPath.c_str(), SoftwareActivationProgressProperties{0});

    softwarePending->softwareActivationProgress->emit_added();

    softwarePending->setActivationBlocksTransition(true);

    softwarePending->setActivation(
        ActivationInterface::Activations::Activating);

    bool success = co_await updateDeviceComponents(components);

    if (success)
    {
        softwarePending->setActivation(
            ActivationInterface::Activations::Active);

        co_await events.generateActivateFailed(softwarePending->objectPath,
                                               componentVersion, false);

        co_await events.generateUpdateSuccessful(softwarePending->objectPath,
                                                 componentVersion);
    }

    softwarePending->setActivationBlocksTransition(false);

    softwarePending->softwareActivationProgress = nullptr;

    if (!success)
    {
        // do not apply the update, it has failed.
        // We can delete the new software version.
        co_await events.generateActivateFailed(softwarePending->objectPath,
                                               componentVersion, true);

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

        co_await events.generateResetRequired(softwarePending->objectPath,
                                              events::HostTransition::Reboot);
    }

    co_return true;
}
