#include "bios_software_manager.hpp"

#include "common/include/dbus_helper.hpp"
#include "common/include/software_manager.hpp"
#include "spi_device.hpp"

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

using namespace phosphor::software;

PHOSPHOR_LOG2_USING;

BIOSSoftwareManager::BIOSSoftwareManager(sdbusplus::async::context& ctx,
                                         bool isDryRun) :
    SoftwareManager(ctx, configTypeBIOS), dryRun(isDryRun)
{}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> BIOSSoftwareManager::initDevice(
    const std::string& service, const std::string& path, SoftwareConfig& config)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::string configIface =
        "xyz.openbmc_project.Configuration." + config.configType;

    std::optional<uint64_t> spiControllerIndex =
        co_await dbusGetRequiredProperty<uint64_t>(
            ctx, service, path, configIface, "SPIControllerIndex");

    if (!spiControllerIndex.has_value())
    {
        error("Missing property: SPIControllerIndex");
        co_return false;
    }

    std::optional<uint64_t> spiDeviceIndex =
        co_await dbusGetRequiredProperty<uint64_t>(
            ctx, service, path, configIface, "SPIDeviceIndex");

    if (!spiDeviceIndex.has_value())
    {
        error("Missing property: SPIDeviceIndex");
        co_return false;
    }

    const std::string configIfaceMux = configIface + ".MuxOutputs";

    std::vector<std::string> names;
    std::vector<uint64_t> values;

    for (size_t i = 0; true; i++)
    {
        const std::string iface = configIfaceMux + std::to_string(i);

        std::optional<std::string> name =
            co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                          iface, "Name");

        std::optional<std::string> polarity =
            co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                          iface, "Polarity");

        if (!name.has_value() || !polarity.has_value())
        {
            break;
        }

        names.push_back(name.value());
        values.push_back((polarity == "High") ? 1 : 0);
    }

    enum FlashLayout layout = flashLayoutFlat;
    enum FlashTool tool = flashToolNone;

    debug("SPI device: {INDEX1}:{INDEX2}", "INDEX1", spiControllerIndex.value(),
          "INDEX2", spiDeviceIndex.value());

    std::unique_ptr<SPIDevice> spiDevice;
    try
    {
        spiDevice = std::make_unique<SPIDevice>(
            ctx, spiControllerIndex.value(), spiDeviceIndex.value(), dryRun,
            names, values, config, this, layout, tool);
    }
    catch (std::exception& e)
    {
        co_return false;
    }

    std::unique_ptr<Software> software =
        std::make_unique<Software>(ctx, *spiDevice);

    // enable this software to be updated
    std::set<RequestedApplyTimes> allowedApplyTimes = {
        RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};

    software->enableUpdate(allowedApplyTimes);

    spiDevice->softwareCurrent = std::move(software);

    spiDevice->softwareCurrent->setVersion(SPIDevice::getVersion());

    devices.insert({config.objectPath, std::move(spiDevice)});

    co_return true;
}
