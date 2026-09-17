#include "spi_factory.hpp"

#include "bios/bios_device.hpp"
#include "common/include/dbus_helper.hpp"
#include "devices/bcm51358.hpp"

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
    GPIOGroup&& resetGPIO, SoftwareConfig& config, SoftwareManager* parent,
    const std::string&, const sdbusplus::object_path&, const std::string&)
{
    co_return std::make_unique<BIOSDevice>(
        ctx, spiControllerIndex, spiDeviceIndex, dryRun, std::move(muxGPIO),
        std::move(resetGPIO), config, parent);
}

static sdbusplus::async::task<std::unique_ptr<SPIDevice>> createBCM51358Device(
    sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
    uint64_t spiDeviceIndex, bool dryRun, GPIOGroup&& muxGPIO,
    GPIOGroup&& resetGPIO, SoftwareConfig& config, SoftwareManager* parent,
    const std::string& service, const sdbusplus::object_path& path,
    const std::string& iface)
{
    std::optional<std::string> port =
        co_await dbusGetRequiredProperty<std::string>(ctx, service, path, iface,
                                                      "SerialPort");
    std::optional<uint64_t> baud = co_await dbusGetRequiredProperty<uint64_t>(
        ctx, service, path, iface, "SerialBaudRate");

    if (!port.has_value() || !baud.has_value())
    {
        error("{TYPE}: Missing serial device config property", "TYPE",
              config.configType);
        co_return nullptr;
    }

    co_return std::make_unique<BCM51358Device>(
        ctx, spiControllerIndex, spiDeviceIndex, dryRun, std::move(muxGPIO),
        std::move(resetGPIO), config, parent, port.value(), baud.value());
}

static const std::unordered_map<
    std::string,
    std::function<sdbusplus::async::task<std::unique_ptr<SPIDevice>>(
        sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
        uint64_t spiDeviceIndex, bool dryRun, GPIOGroup&& muxGPIO,
        GPIOGroup&& resetGPIO, SoftwareConfig& config, SoftwareManager* parent,
        const std::string& service, const sdbusplus::object_path& path,
        const std::string& iface)>>
    supportedSpiChips = {{"IntelHostSPIFlash", createBIOSDevice},
                         {"HostSPIFlash", createBIOSDevice},
                         {"BCM51358Firmware", createBCM51358Device}};

sdbusplus::async::task<std::unique_ptr<SPIDevice>> SPIFactory::create(
    const std::string& chipType, sdbusplus::async::context& ctx,
    uint64_t spiControllerIndex, uint64_t spiDeviceIndex, bool dryRun,
    GPIOGroup&& muxGPIO, GPIOGroup&& resetGPIO, SoftwareConfig& config,
    SoftwareManager* parent, const std::string& service,
    const sdbusplus::object_path& path, const std::string& iface)
{
    const auto it = supportedSpiChips.find(chipType);
    if (it != supportedSpiChips.end())
    {
        try
        {
            co_return co_await it->second(
                ctx, spiControllerIndex, spiDeviceIndex, dryRun,
                std::move(muxGPIO), std::move(resetGPIO), config, parent,
                service, path, iface);
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
