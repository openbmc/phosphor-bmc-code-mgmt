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

bool BIOSSoftwareManager::isSupported(const std::string& configType)
{
    return configType == "IntelHostSPIFlash" || configType == "HostSPIFlash";
}

sdbusplus::async::task<bool> BIOSSoftwareManager::initDevice(
    const std::string& service, const sdbusplus::object_path& path,
    SoftwareConfig& config)
{
    auto spiControllerIndex =
        config.getProperty<uint64_t>("SPIControllerIndex");
    if (!spiControllerIndex.has_value())
    {
        error("Missing property: SPIControllerIndex");
        co_return false;
    }

    auto spiDeviceIndex = config.getProperty<uint64_t>("SPIDeviceIndex");
    if (!spiDeviceIndex.has_value())
    {
        error("Missing property: SPIDeviceIndex");
        co_return false;
    }

    enum FlashTool tool = flashToolNone;

    if (config.configType == "IntelHostSPIFlash")
    {
        tool = flashToolFlashrom;
    }
    else if (config.configType == "HostSPIFlash")
    {
        tool = flashToolFlashcp;
    }

    const std::string configIfaceMux = config.baseInterface + ".MuxOutputs";

    std::vector<std::string> names;
    std::vector<bool> values;

    for (size_t i = 0; true; i++)
    {
        const std::string iface = configIfaceMux + std::to_string(i);

        auto name = config.getProperty<std::string>(iface, "Name");
        if (!name.has_value())
        {
            name = co_await dbusGetRequiredProperty<std::string>(
                ctx, service, path.str, iface, "Name");
        }

        auto polarity = config.getProperty<std::string>(iface, "Polarity");
        if (!polarity.has_value())
        {
            polarity = co_await dbusGetRequiredProperty<std::string>(
                ctx, service, path.str, iface, "Polarity");
        }

        if (!name.has_value() || !polarity.has_value())
        {
            break;
        }

        names.push_back(name.value());
        values.push_back((polarity == "High") ? 1 : 0);
    }

    enum FlashLayout layout = flashLayoutFlat;

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

    spiDevice->softwareCurrent->setVersion(
        SPIDevice::getVersion(), SoftwareVersion::VersionPurpose::Host);

    devices.insert({config.objectPath, std::move(spiDevice)});

    co_return true;
}
