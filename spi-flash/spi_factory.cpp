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

static sdbusplus::async::task<std::unique_ptr<SPIDevice>> createBIOSDevice(
    sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
    uint64_t spiDeviceIndex, bool dryRun, GPIOGroup&& muxGPIO,
    SoftwareConfig& config, SoftwareManager* parent,
    const std::string& /*unused*/, const sdbusplus::object_path& /*unused*/,
    const std::string& /*unused*/)
{
    co_return std::make_unique<BIOSDevice>(
        ctx, spiControllerIndex, spiDeviceIndex, dryRun, std::move(muxGPIO),
        config, parent);
}

static sdbusplus::async::task<std::unique_ptr<SPIDevice>> createE810Device(
    sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
    uint64_t spiDeviceIndex, bool dryRun, const std::vector<std::string>& names,
    const std::vector<bool>& values, SoftwareConfig& config,
    SoftwareManager* parent, const std::string& /*unused*/,
    const sdbusplus::object_path& /*unused*/, const std::string& /*unused*/)
{
    co_return std::make_unique<E810Device>(
        ctx, spiControllerIndex, spiDeviceIndex, dryRun, names, values, config,
        parent);
}

static const std::unordered_map<
    std::string,
    std::function<sdbusplus::async::task<std::unique_ptr<SPIDevice>>(
        sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
        uint64_t spiDeviceIndex, bool dryRun, GPIOGroup&& muxGPIO,
        SoftwareConfig& config, SoftwareManager* parent,
        const std::string& service, const sdbusplus::object_path& path,
        const std::string& iface)>>
    supportedSpiChips = {{"IntelHostSPIFlash", createBIOSDevice},
                         {"HostSPIFlash", createBIOSDevice},
                         {"IntelE810SPIFlash", createE810Device}};

sdbusplus::async::task<std::unique_ptr<SPIDevice>> SPIFactory::create(
    const std::string& chipType, sdbusplus::async::context& ctx,
    uint64_t spiControllerIndex, uint64_t spiDeviceIndex, bool dryRun,
    GPIOGroup&& muxGPIO, SoftwareConfig& config, SoftwareManager* parent,
    const std::string& service, const sdbusplus::object_path& path,
    const std::string& configIface)
{
    const auto it = supportedSpiChips.find(chipType);
    if (it != supportedSpiChips.end())
    {
        try
        {
            co_return co_await it->second(
                ctx, spiControllerIndex, spiDeviceIndex, dryRun,
                std::move(muxGPIO), config, parent, service, path, configIface);
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
