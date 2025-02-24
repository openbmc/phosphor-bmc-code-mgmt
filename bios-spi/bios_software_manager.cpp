#include "bios_software_manager.hpp"

#include "common/include/software_manager.hpp"
#include "spi_device.hpp"

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

using namespace phosphor::software;

PHOSPHOR_LOG2_USING;

template <typename T>
sdbusplus::async::task<std::optional<T>> dbusGetRequiredProperty(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& path, const std::string& intf,
    const std::string& property)
{
    auto client =
        sdbusplus::async::proxy().service(service).path(path).interface(
            "org.freedesktop.DBus.Properties");

    std::optional<T> opt = std::nullopt;
    try
    {
        std::variant<T> result =
            co_await client.call<std::variant<T>>(ctx, "Get", intf, property);

        opt = std::get<T>(result);
    }
    catch (std::exception& e)
    {
        error(
            "[config] missing property {PROPERTY} on path {PATH}, interface {INTF}",
            "PROPERTY", property, "PATH", path, "INTF", intf);
    }
    co_return opt;
}

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

    std::optional<int64_t> spiControllerIndex =
        co_await dbusGetRequiredProperty<uint64_t>(
            ctx, service, path, configIface, "SPIControllerIndex");

    std::optional<int64_t> spiDeviceIndex =
        co_await dbusGetRequiredProperty<uint64_t>(
            ctx, service, path, configIface, "SPIDeviceIndex");

    std::optional<bool> hasME = co_await dbusGetRequiredProperty<bool>(
        ctx, service, path, configIface, "HasME");

    std::optional<std::string> configLayout =
        co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                      configIface, "Layout");

    std::optional<std::string> configTool =
        co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                      configIface, "Tool");

    std::optional<std::vector<std::string>> configGPIONames =
        co_await dbusGetRequiredProperty<std::vector<std::string>>(
            ctx, service, path, configIface, "MuxOutputs");
    std::optional<std::vector<uint64_t>> configGPIOValues =
        co_await dbusGetRequiredProperty<std::vector<uint64_t>>(
            ctx, service, path, configIface, "MuxGPIOValues");

    if (!spiControllerIndex.has_value() || !spiDeviceIndex.has_value() ||
        !configGPIONames.has_value() || !configGPIOValues.has_value() ||
        !configLayout.has_value() || !configTool.has_value() ||
        !hasME.has_value())
    {
        info("[config] error: missing property");
        co_return false;
    }

    if (configGPIONames.value().size() != configGPIOValues.value().size())
    {
        error(
            "mismatch: configured gpios ({N1}) and their values ({N2}) have different size",
            "N1", configGPIONames.value().size(), "N2",
            configGPIOValues.value().size());
        co_return false;
    }

    enum FLASH_LAYOUT layout;
    if (configLayout.value() == "Flat")
    {
        layout = FLASH_LAYOUT_FLAT;
    }
    else if (configLayout.value() == "IFD")
    {
        layout = FLASH_LAYOUT_INTEL_FLASH_DESCRIPTOR;
    }
    else
    {
        error("[config] unsupported flash layout config: {OPTION}", "OPTION",
              configLayout.value());
        info("supported options: 'Flat', 'IFD'");
        co_return false;
    }

    enum FLASH_TOOL tool;
    if (configTool.value() == "flashrom")
    {
        tool = FLASH_TOOL_FLASHROM;
    }
    else if (configTool.value() == "None")
    {
        tool = FLASH_TOOL_NONE;
    }
    else
    {
        error("[config] unsupported Tool: {OPTION}", "OPTION",
              configTool.value());
        co_return false;
    }

    debug("[config] spi device: {INDEX1}:{INDEX2}", "INDEX1",
          spiControllerIndex.value(), "INDEX2", spiDeviceIndex.value());

    auto spiDevice = std::make_unique<SPIDevice>(
        ctx, spiControllerIndex.value(), spiDeviceIndex.value(), dryRun,
        hasME.value(), configGPIONames.value(), configGPIOValues.value(),
        config, this, layout, tool);

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
