#include "example_device.hpp"

#include "common/include/device.hpp"
#include "common/include/software_config.hpp"
#include "common/include/software_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <memory>

PHOSPHOR_LOG2_USING;

SoftwareConfig ExampleDevice::defaultConfig =
    SoftwareConfig(exampleInvObjPath, exampleVendorIANA,
                   exampleCompatibleHardware, "Nop", exampleName);

long ExampleCodeUpdater::getRandomId()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    unsigned int seed = ts.tv_nsec ^ getpid();
    srandom(seed);
    return random() % 10000;
}

// nop code updater needs unique suffix on dbus for parallel unit testing
ExampleCodeUpdater::ExampleCodeUpdater(sdbusplus::async::context& ctx,
                                       long uniqueSuffix) :
    SoftwareManager(ctx, "ExampleUpdater" + std::to_string(uniqueSuffix))
{}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<> ExampleCodeUpdater::initDevice(
    const std::string& /*unused*/, const std::string& /*unused*/,
    SoftwareConfig& /*unused*/)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto device = std::make_unique<ExampleDevice>(ctx, this);

    device->currentSoftware = std::make_unique<Software>(ctx, *device);

    device->currentSoftware->setVersion("v1.0");
    device->currentSoftware->setActivation(
        SoftwareActivation::Activations::Active);

    auto applyTimes = {RequestedApplyTimes::OnReset};
    device->currentSoftware->enableUpdate(applyTimes);

    devices.insert({exampleInvObjPath, std::move(device)});

    co_return;
}

ExampleDevice::ExampleDevice(sdbusplus::async::context& ctx,
                             SoftwareManager* parent,
                             const SoftwareConfig& config) :
    Device(ctx, config, parent,
           {RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset})
{}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> ExampleDevice::updateDevice(
    const uint8_t* /*unused*/, size_t compImageSize)
// NOLINTEND(readability-static-accessed-through-instance)
{
    debug("Called device specific update function with image size {SIZE}",
          "SIZE", compImageSize);

    deviceSpecificUpdateFunctionCalled = true;

    // Setting this property for demonstration purpose.
    // For a real device, this could represent the
    // percentage completion of writing the firmware,
    // and any progress made in the update process within this function.
    // There is no hard constraint on the values here,
    // we do not have to reach any specific percentage.
    // The percentage should be monotonic and increasing.
    for (auto progress = 0; progress <= 100; progress += 20)
    {
        setUpdateProgress(90);
    }

    co_return true;
}
