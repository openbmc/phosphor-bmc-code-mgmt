#include "i2c_vr_device.hpp"

#include "common/include/device.hpp"
#include "common/include/software_manager.hpp"
#include "regulators/vr.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>

I2CVRDevice::I2CVRDevice(sdbusplus::async::context& ctx, bool dryRun,
                         const std::string& vrChipName, const uint16_t& bus,
                         const uint8_t& addr, DeviceConfig& config,
                         SoftwareManager* parent) :
    Device(ctx, dryRun, config, parent), busNo(bus), address(addr),
    vrTypeStr(vrChipName)

{
    lg2::debug(
        "initialized I2C device(on I2C bus: {BUS} with address: {ADDR} instance on dbus",
        "BUS", busNo, "ADDR", address);
    lg2::debug("vr chip string: {STR}", "STR", vrTypeStr);
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<std::string> I2CVRDevice::getInventoryItemObjectPath()
// NOLINTEND(readability-static-accessed-through-instance)
{
    co_return this->config.objPathConfig;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> I2CVRDevice::deviceSpecificUpdateFunction(
    const uint8_t* image, size_t image_size,
    std::unique_ptr<SoftwareActivationProgress>& activationProgress)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::unique_ptr<VR::VoltageRegulator> inf = VR::create(vrTypeStr, busNo, address);
    // Update loop, contition is size of image and reduced by size written to
    // chip.
    if (dryRun)
    {
        for (int i = 0; i < 10; i++)
        {
            // acticationProgress->progress(1...100);
            (void)image;
            (void)image_size;
            activationProgress->progress(i * 10);
            usleep(500000);
            lg2::info("[I2CVR] dry run, not writing to voltage regulator");
        }
    }
    else
    {
        (void)image;
        (void)image_size;
    }

    lg2::info("Update of VR done. Exiting...");
    co_return true;
}
