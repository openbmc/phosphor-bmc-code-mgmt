#include "nopdevice.hpp"

#include "common/include/device.hpp"
#include "common/include/device_config.hpp"
#include "common/include/software_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <memory>

static long getRandomId()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    unsigned int seed = ts.tv_nsec ^ getpid();
    srandom(seed);
    return random() % 10000;
}

// nop code updater needs unique suffix on dbus for parallel unit testing
NopCodeUpdater::NopCodeUpdater(sdbusplus::async::context& ctx) :
    NopCodeUpdater(ctx, getRandomId())
{}

NopCodeUpdater::NopCodeUpdater(sdbusplus::async::context& ctx,
                               long uniqueSuffix) :
    SoftwareManager(ctx, "Nop" + std::to_string(uniqueSuffix), true)
{}

// NOLINTBEGIN
sdbusplus::async::task<> NopCodeUpdater::getInitialConfigurationSingleDevice(
    const std::string& /*unused*/, const std::string& /*unused*/,
    DeviceConfig& /*unused*/)
// NOLINTEND
{
    co_return;
}

NopDevice::NopDevice(sdbusplus::async::context& ctx, const DeviceConfig& config,
                     SoftwareManager* parent) :
    Device(ctx, true, config, parent)
{}
NopDevice::NopDevice(sdbusplus::async::context& ctx, SoftwareManager* parent) :
    Device(ctx, true,
           DeviceConfig(exampleVendorIANA, exampleCompatible, "Nop",
                        exampleName),
           parent)
{}

// NOLINTBEGIN
sdbusplus::async::task<bool> NopDevice::updateDevice(
    const uint8_t* /*unused*/, size_t compImageSize,
    std::unique_ptr<SoftwareActivationProgress>& activationProgress)
// NOLINTEND
{
    lg2::debug(
        "[nop device] called device specific update function with image size {SIZE}",
        "SIZE", compImageSize);

    deviceSpecificUpdateFunctionCalled = true;

    // Setting this property for demonstration purpose.
    // For a real device, this could represent the
    // percentage completion of writing the firmware,
    // and any progress made in the update process within this function.
    activationProgress->progress(30);

    // There is no hard constraint on the values here,
    // we do not have to reach any specific percentage.
    // The percentage should be monotonic and increasing.
    activationProgress->progress(90);

    co_return true;
}

// NOLINTBEGIN
sdbusplus::async::task<std::string> NopDevice::getInventoryItemObjectPath()
// NOLINTEND
{
    // for a real device, it could find its inventory item via associations and
    // the configuration interface, or go via the mapper to find the
    // specific inventory item interface and then get the path from there.

    co_return "/xyz/openbmc_project/inventory/item/my_nop_item";
}
