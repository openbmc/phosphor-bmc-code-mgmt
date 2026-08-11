#include "bios_software_manager.hpp"

#include "common/include/dbus_helper.hpp"
#include "common/include/software_manager.hpp"
#include "spi_bios.hpp"

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

sdbusplus::async::task<bool> BIOSSoftwareManager::initDevice(
    const std::string& service, const sdbusplus::object_path& path,
    SoftwareConfig& config)
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

    enum FlashTool tool = flashToolNone;

    if (config.configType == "IntelHostSPIFlash")
    {
        tool = flashToolFlashrom;
    }
    else if (config.configType == "HostSPIFlash")
    {
        tool = flashToolFlashcp;
    }

    GPIOGroup muxGPIO = co_await dbusGetGPIOs(
        ctx, service, path, configIface + ".MuxOutputs", "Mux");

    enum FlashLayout layout = flashLayoutFlat;

    debug("SPI device: {INDEX1}:{INDEX2}", "INDEX1", spiControllerIndex.value(),
          "INDEX2", spiDeviceIndex.value());

    std::unique_ptr<SPIDevice> spiDevice;
    try
    {
        spiDevice = std::make_unique<SPIBIOS>(
            ctx, spiControllerIndex.value(), spiDeviceIndex.value(), dryRun,
            std::move(muxGPIO), config, this, layout, tool);
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
        spiDevice->getVersion(), SoftwareVersion::VersionPurpose::Host);

    devices.insert({config.objectPath, std::move(spiDevice)});

    co_return true;
}
