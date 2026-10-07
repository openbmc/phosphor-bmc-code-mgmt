#include "software_update.hpp"

#include "device.hpp"
#include "software.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>
#include <xyz/openbmc_project/Software/RebootBlocksNewUpdates/common.hpp>
#include <xyz/openbmc_project/Software/Update/aserver.hpp>

#include <string>
#include <vector>

PHOSPHOR_LOG2_USING;

using Unavailable = sdbusplus::xyz::openbmc_project::Common::Error::Unavailable;

using namespace phosphor::logging;
using namespace phosphor::software::update;
using namespace phosphor::software::device;
using namespace phosphor::software;

namespace SoftwareLogging = phosphor::logging::xyz::openbmc_project::software;
namespace SoftwareErrors =
    sdbusplus::error::xyz::openbmc_project::software::image;

SoftwareUpdate::SoftwareUpdate(
    sdbusplus::async::context& ctx, const sdbusplus::object_path& path,
    Software& software,
    const std::set<RequestedApplyTimes>& allowedApplyTimes) :
    sdbusplus::aserver::xyz::openbmc_project::software::Update<SoftwareUpdate>(
        ctx, path),
    ctx(ctx), software(software), allowedApplyTimes(allowedApplyTimes)
{
    emit_added();
}

SoftwareUpdate::~SoftwareUpdate()
{
    emit_removed();
}

auto SoftwareUpdate::method_call(start_update_t /*unused*/, auto image,
                                 auto applyTime)
    -> sdbusplus::async::task<start_update_t::return_type>
{
    debug("Requesting Image update with {FD}", "FD", image.fd);

    Device& device = software.parentDevice;

    if (device.updateInProgress)
    {
        error("An update is already in progress, cannot update.");
        elog<Unavailable>();
    }

    // Refuse to start a new update while a BMC reboot is pending. An updater
    // exposes Software.RebootBlocksNewUpdates while its reboot is deferred;
    // starting new updates now could keep extending that wait.
    {
        using RebootBlocksNewUpdates = sdbusplus::common::xyz::openbmc_project::
            software::RebootBlocksNewUpdates;
        auto mapper =
            sdbusplus::client::xyz::openbmc_project::ObjectMapper<>(ctx)
                .service("xyz.openbmc_project.ObjectMapper")
                .path("/xyz/openbmc_project/object_mapper");
        std::vector<std::string> blockers;
        try
        {
            blockers = co_await mapper.get_sub_tree_paths(
                "/xyz/openbmc_project/software", 0,
                {RebootBlocksNewUpdates::interface});
        }
        catch (const sdbusplus::exception_t&)
        {
            // The mapper returns an error when no object implements the
            // interface; that just means nothing is blocking, so proceed.
        }
        if (!blockers.empty())
        {
            error("A BMC reboot is pending; refusing to start a new update.");
            elog<Unavailable>();
        }
    }

    device.updateInProgress = true;

    // check if the apply time is allowed by our device
    if (!allowedApplyTimes.contains(applyTime))
    {
        error(
            "the selected apply time {APPLYTIME} is not allowed by the device",
            "APPLYTIME", applyTime);
        device.updateInProgress = false;
        using Argument =
            phosphor::logging::xyz::openbmc_project::common::InvalidArgument;
        elog<sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument>(
            Argument::ARGUMENT_NAME("ApplyTime"),
            Argument::ARGUMENT_VALUE(
                sdbusplus::message::convert_to_string(applyTime).c_str()));
    }

    debug("started asynchronous update with fd {FD}", "FD", image.fd);

    int imageDup = dup(image.fd);

    if (imageDup < 0)
    {
        error("ERROR calling dup on fd: {ERR}", "ERR", strerror(errno));
        device.updateInProgress = false;
        co_return software.objectPath;
    }

    debug("starting async update with FD: {FD}\n", "FD", imageDup);

    std::unique_ptr<Software> softwareInstance;
    try
    {
        softwareInstance = std::make_unique<Software>(ctx, device);
    }
    catch (const std::exception& e)
    {
        error("Failed to create software object during update: {ERROR}",
              "ERROR", e);
        device.updateInProgress = false;
        close(imageDup);
        co_return software.objectPath;
    }

    softwareInstance->setActivation(ActivationInterface::Activations::NotReady);

    std::string newObjPath = softwareInstance->objectPath;

    ctx.spawn(
        [](Device& device, int imageDup, RequestedApplyTimes applyTime,
           std::unique_ptr<Software> swupdate) -> sdbusplus::async::task<> {
            co_await device.startUpdateAsync(imageDup, applyTime,
                                             std::move(swupdate));
            device.updateInProgress = false;
            close(imageDup);
            co_return;
        }(device, imageDup, applyTime, std::move(softwareInstance)));

    // We need the object path for the new software here.
    // It must be the same as constructed during the update process.
    // This is so that bmcweb and redfish clients can keep track of the update
    // process.
    co_return newObjPath;
}

auto SoftwareUpdate::get_property(allowed_apply_times_t /*unused*/) const
{
    return allowedApplyTimes;
}
