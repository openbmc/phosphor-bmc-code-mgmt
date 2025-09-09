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

using namespace phosphor::software;
using namespace phosphor::software::config;
using namespace phosphor::software::manager;
using namespace phosphor::software::device;
using namespace phosphor::software::example_device;

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

sdbusplus::async::task<bool> ExampleCodeUpdater::initDevice(
    const std::string& /*unused*/, const std::string& /*unused*/,
    SoftwareConfig& /*unused*/)
{
    auto device = std::make_unique<ExampleDevice>(ctx, this);

    device->softwareCurrent = std::make_unique<Software>(ctx, *device);

    device->softwareCurrent->setVersion("v1.0",
                                        SoftwareVersion::VersionPurpose::Other);
    device->softwareCurrent->setActivation(
        SoftwareActivation::Activations::Active);

    auto applyTimes = {RequestedApplyTimes::OnReset};
    device->softwareCurrent->enableUpdate(applyTimes);

    devices.insert({exampleInvObjPath, std::move(device)});

    co_return true;
}

ExampleDevice::ExampleDevice(sdbusplus::async::context& ctx,
                             SoftwareManager* parent,
                             const SoftwareConfig& config) :
    Device(ctx, config, parent,
           {RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset})
{}

sdbusplus::async::task<bool> ExampleDevice::updateDevice(
    const uint8_t* /*unused*/, size_t compImageSize)
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
        setUpdateProgress(progress);
    }

    co_return true;
}
