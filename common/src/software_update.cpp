#include "software_update.hpp"

#include "device.hpp"
#include "software.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Software/Update/aserver.hpp>

SoftwareUpdate::SoftwareUpdate(
    sdbusplus::async::context& ctx, const char* path, Software& software,
    const std::set<RequestedApplyTimes>& allowedApplyTimes) :
    sdbusplus::aserver::xyz::openbmc_project::software::Update<SoftwareUpdate>(
        ctx, path),
    software(software), allowedApplyTimes(allowedApplyTimes)
{}

auto SoftwareUpdate::method_call(start_update_t /*unused*/, auto image,
                                 auto applyTime)
    -> sdbusplus::async::task<start_update_t::return_type>
{
    lg2::info("requesting Device update");

    if (SoftwareUpdate::updateInProgress)
    {
        lg2::error("An update is already in progress, cannot update.");
        co_return sdbusplus::message::object_path();
    }

    SoftwareUpdate::updateInProgress = true;

    // check if the apply time is allowed by our device
    if (!this->allowedApplyTimes.contains(applyTime))
    {
        lg2::error(
            "the selected apply time {APPLYTIME} is not allowed by the device",
            "APPLYTIME", applyTime);
        SoftwareUpdate::updateInProgress = false;
        co_return sdbusplus::message::object_path();
    }

    lg2::info("started asynchronous update with fd {FD}", "FD", image.fd);

    Device& device = this->software.getParentDevice();

    int imageDup = dup(image.fd);

    if (imageDup < 0)
    {
        lg2::error("ERROR calling dup on fd: {ERR}", "ERR", strerror(errno));
        SoftwareUpdate::updateInProgress = false;
        co_return this->software.getObjectPath();
    }

    lg2::debug("starting async update with FD: {FD}\n", "FD", imageDup);

    std::unique_ptr<Software> softwareInstance =
        std::make_unique<Software>(ctx, device);

    softwareInstance->setActivation(ActivationInterface::Activations::NotReady);

    std::string newObjPath = softwareInstance->getObjectPath();

    // NOLINTBEGIN(readability-static-accessed-through-instance)
    ctx.spawn(
        [](Device& device, int imageDup, RequestedApplyTimes applyTime,
           std::unique_ptr<Software> swupdate) -> sdbusplus::async::task<> {
            co_await device.startUpdateAsync(imageDup, applyTime,
                                             std::move(swupdate));
            SoftwareUpdate::updateInProgress = false;
            co_return;
        }(device, imageDup, applyTime, std::move(softwareInstance)));
    // NOLINTEND

    // We need the object path for the new software here.
    // It must be the same as constructed during the update process.
    // This is so that bmcweb and redfish clients can keep track of the update
    // process.
    co_return newObjPath;
}

auto SoftwareUpdate::get_property(allowed_apply_times_t /*unused*/) const
{
    return this->allowedApplyTimes;
}
