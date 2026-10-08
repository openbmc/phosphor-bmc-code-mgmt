#include "spi_factory.hpp"

#include "bios/bios_device.hpp"
#include "e810/e810_device.hpp"

namespace phosphor::software::manager
{

SPIFactory& SPIFactory::instance()
{
    static SPIFactory factory;
    return factory;
}

static sdbusplus::async::task<std::optional<SPIDeviceConfig>>
    getSPIDeviceConfig(sdbusplus::async::context& ctx,
                       const std::string& service,
                       const sdbusplus::object_path& path,
                       const std::string& configIface)
{
    std::optional<uint64_t> spiControllerIndex =
        co_await dbusGetRequiredProperty<uint64_t>(
            ctx, service, path, configIface, "SPIControllerIndex");

    if (!spiControllerIndex.has_value())
    {
        error("Missing property: SPIControllerIndex");
        co_return std::nullopt;
    }

    std::optional<uint64_t> spiDeviceIndex =
        co_await dbusGetRequiredProperty<uint64_t>(
            ctx, service, path, configIface, "SPIDeviceIndex");

    if (!spiDeviceIndex.has_value())
    {
        error("Missing property: SPIDeviceIndex");
        co_return std::nullopt;
    }

    debug("SPI device: {INDEX1}:{INDEX2}", "INDEX1", spiControllerIndex.value(),
          "INDEX2", spiDeviceIndex.value());

    co_return SPIDeviceConfig{spiControllerIndex.value(),
                              spiDeviceIndex.value()};
}

template <class T>
static sdbusplus::async::task<std::unique_ptr<SPIDevice>>
    createGenericSPIDevice(sdbusplus::async::context& ctx,
                           const std::string& service,
                           const sdbusplus::object_path& path, bool dryRun,
                           SoftwareConfig& config, SoftwareManager* parent)
{
    std::string configIface =
        "xyz.openbmc_project.Configuration." + config.configType;
    std::optional<SPIDeviceConfig> spiConfig =
        co_await getSPIDeviceConfig(ctx, service, path, configIface);

    if (!spiConfig.has_value())
    {
        co_return nullptr;
    }

    GPIOGroup muxGPIO = co_await dbusGetGPIOs(
        ctx, service, path, configIface + ".MuxOutputs", "Mux");

    co_return std::make_unique<T>(ctx, spiConfig.value(), dryRun,
                                  std::move(muxGPIO), config, parent);
}

static const std::unordered_map<
    std::string,
    std::function<sdbusplus::async::task<std::unique_ptr<SPIDevice>>(
        sdbusplus::async::context& ctx, const std::string& service,
        const sdbusplus::object_path& path, bool dryRun, SoftwareConfig& config,
        SoftwareManager* parent)>>
    supportedSpiChips = {
        {"IntelHostSPIFlash", createGenericSPIDevice<BIOSDevice>},
        {"HostSPIFlash", createGenericSPIDevice<BIOSDevice>},
        {"IntelE810SPIFlash", createGenericSPIDevice<E810Device>}};

sdbusplus::async::task<std::unique_ptr<SPIDevice>> SPIFactory::create(
    sdbusplus::async::context& ctx, const std::string& service,
    const sdbusplus::object_path& path, bool dryRun, SoftwareConfig& config,
    SoftwareManager* parent)
{
    const std::string& chipType = config.configType;
    const auto it = supportedSpiChips.find(chipType);
    if (it != supportedSpiChips.end())
    {
        try
        {
            co_return co_await it->second(ctx, service, path, dryRun, config,
                                          parent);
        }

        catch (const std::exception& e)
        {
            error("Failed to create {TYPE}: {ERROR}", "TYPE", chipType, "ERROR",
                  e.what());
            co_return nullptr;
        }
    }

    error("Unsupported SPI device type: {TYPE}", "TYPE", chipType);
    co_return nullptr;
}

std::vector<std::string> SPIFactory::getConfigInterfaceNames()
{
    std::vector<std::string> configs;
    configs.reserve(supportedSpiChips.size());
    for (const auto& chipEnum : supportedSpiChips)
    {
        configs.push_back(chipEnum.first);
    }
    return configs;
}

} // namespace phosphor::software::manager
