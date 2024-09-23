#include "device.hpp"

#include "fw-update/common/fw_manager.hpp"
#include "fw-update/common/software.hpp"
#include "sdbusplus/async/timer.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

Device::Device(sdbusplus::async::context& io, bool isDryRun,
               const std::string& vendorIANA, const std::string& compatible,
               FWManager* parent) :
    parent(parent), dryRun(isDryRun), bus(io.get_bus()), io(io),
    vendorIANA(vendorIANA), compatible(compatible)
{}

void Device::deleteOldSw(const std::string& swid)
{
    lg2::info("deleting old sw version {SWID}", "SWID", swid);

    // this should also take down the old dbus interfaces

    if (softwareCurrent->swid == swid)
    {
        softwareCurrent = nullptr;
    }
    if (softwareUpdate->swid == swid)
    {
        softwareUpdate = nullptr;
    }
}

void Device::startUpdate(sdbusplus::message::unix_fd image,
                         sdbusplus::common::xyz::openbmc_project::software::
                             ApplyTime::RequestedApplyTimes applyTime,
                         const std::string& oldSwId)
{
    this->image = image;
    this->applyTime = applyTime;
    this->oldSwId = oldSwId;

    const int timeout = 5;
    lg2::debug("starting timer for async update in {SECONDS}s\n", "SECONDS",
               timeout);

    io.spawn([](sdbusplus::async::context& io, Device* device,
                const int timeout) -> sdbusplus::async::task<> {
        co_await sdbusplus::async::sleep_for(io, std::chrono::seconds(timeout));
        lg2::debug("update timer expired (new coro)");
        co_await device->startUpdateAsync();
        co_return;
    }(io, this, timeout));

    lg2::debug("update timer has started");
}

const auto actNotReady = sdbusplus::common::xyz::openbmc_project::software::
    Activation::Activations::NotReady;
const auto actInvalid = sdbusplus::common::xyz::openbmc_project::software::
    Activation::Activations::Invalid;
const auto actReady = sdbusplus::common::xyz::openbmc_project::software::
    Activation::Activations::Ready;
const auto actActivating = sdbusplus::common::xyz::openbmc_project::software::
    Activation::Activations::Activating;

const auto applyTimeImmediate = sdbusplus::common::xyz::openbmc_project::
    software::ApplyTime::RequestedApplyTimes::Immediate;

sdbusplus::async::task<bool> Device::continueUpdate(
    sdbusplus::message::unix_fd image,
    sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
        RequestedApplyTimes applyTime,
    const std::string& oldSwId)
{
    std::shared_ptr<Software> newsw = softwareUpdate;

    // design: Create Interface xyz.openbmc_project.Software.Activation at
    // ObjectPath with Status = NotReady

    newsw->softwareActivation.activation(actNotReady);

    const bool imageVerificationSuccess = parent->verifyImage(image);

    if (!imageVerificationSuccess)
    {
        lg2::error("failed to verify image");

        newsw->softwareActivation.activation(actInvalid);
        co_return false;
    }

    lg2::info("successfully verified the image");

    newsw->softwareActivation.activation(actReady);

    // design: Create Interface xyz.openbmc_project.Software.Version at
    // ObjectPath this is already done by our class constructor

    // (design): Create Interface
    // xyz.openbmc_project.Software.ActivationProgress at ObjectPath
    newsw->optSoftwareActivationProgress =
        std::make_shared<SoftwareActivationProgress>(
            io, FWManager::getObjPathFromSwid(newsw->swid).c_str());

    // (design): Create Interface
    // xyz.openbmc_project.Software.ActivationBlocksTransition at ObjectPath
    newsw->optSoftwareActivationBlocksTransition =
        std::make_shared<SoftwareActivationBlocksTransition>(
            io, FWManager::getObjPathFromSwid(newsw->swid).c_str());

    // design: Activation.Status = Activating
    newsw->softwareActivation.activation(actActivating);

    // design: we start the update here
    bool success = co_await deviceSpecificUpdateFunction(
        image, newsw->optSoftwareActivationProgress);

    // design: we finished the update here
    newsw->softwareActivation.activation(actActive);

    // (design):Delete Interface
    // xyz.openbmc_project.Software.ActivationBlocksTransition
    newsw->optSoftwareActivationBlocksTransition = nullptr;
    // (design):Delete Interface
    // xyz.openbmc_project.Software.ActivationProgress
    newsw->optSoftwareActivationProgress = nullptr;

    if (!success)
    {
        // do not apply the update, it has failed
        co_return false;
    }

    if (applyTime == applyTimeImmediate)
    {
        // TODO(design): reset device
        // TODO(design): update functional association to system inventory item

        // TODO(design): Create Interface xyz.openbmc_project.Software.Update at
        // ObjectPath
        //  this is already done by our class constructor, but we should defer
        //  that to later since this sw has only been applied at this point in
        //  the code

        // design: Delete all interfaces on previous ObjectPath
        // makes sense, we do not want the old sw version to be updated or
        // displayed, since it is not the active version

        deleteOldSw(oldSwId);

        // TODO(design): Create active association to System Inventory Item
    }

    co_return true;
}
