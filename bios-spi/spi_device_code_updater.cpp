#include "spi_device_code_updater.hpp"

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

SPIDeviceCodeUpdater::SPIDeviceCodeUpdater(sdbusplus::async::context& ctx,
                                           bool isDryRun) :
    SoftwareManager(ctx, configTypeSPIDevice), dryRun(isDryRun)
{}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDeviceCodeUpdater::initDevice(
    const std::string& service, const std::string& path, SoftwareConfig& config)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::string configIface =
        "xyz.openbmc_project.Configuration." + config.configType;

    std::optional<std::string> optSpiPath =
        co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                      configIface, "Path");

    std::optional<bool> optHasME = co_await dbusGetRequiredProperty<bool>(
        ctx, service, path, configIface, "HasME");

    std::optional<std::string> optLayout =
        co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                      configIface, "Layout");

    std::optional<std::string> optTool =
        co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                      configIface, "Tool");

    if (!optSpiPath.has_value() || !optHasME.has_value())
    {
        co_return false;
    }

    std::string spiPath = optSpiPath.value();
    bool hasME = optHasME.value();

    debug("[config] spi device: {SPIDEV}", "SPIDEV", spiPath);

    std::optional<std::vector<std::string>> optGpioNames =
        co_await dbusGetRequiredProperty<std::vector<std::string>>(
            ctx, service, path, configIface, "MuxOutputs");
    std::optional<std::vector<uint64_t>> optGpioValues =
        co_await dbusGetRequiredProperty<std::vector<uint64_t>>(
            ctx, service, path, configIface, "MuxGPIOValues");

    if (!optGpioNames.has_value() || !optGpioValues.has_value())
    {
        co_return false;
    }

    std::vector<std::string> gpioLines;
    std::vector<uint8_t> gpioValues;

    if (optGpioNames.value().size() != optGpioValues.value().size())
    {
        error(
            "mismatch: configured gpios ({N1}) and their values ({N2}) have different size",
            "N1", optGpioNames.value().size(), "N2",
            optGpioValues.value().size());
        co_return false;
    }

    for (size_t i = 0; i < optGpioNames.value().size(); i++)
    {
        gpioLines.push_back(optGpioNames.value()[i]);
        gpioValues.push_back(optGpioValues.value()[i]);

        debug("[config] gpio {NAME} = {VALUE}", "NAME", optGpioNames.value()[i],
              "VALUE", optGpioValues.value()[i]);
    }

    debug("[config] extracted {N} gpios from EM config", "N", gpioLines.size());

    bool layoutFlat;

    if (!optLayout.has_value())
    {
        info("[config] error: no flash layout chosen (property 'Layout')");
        co_return false;
    }

    const std::string& layout = optLayout.value();
    if (layout == "Flat")
    {
        layoutFlat = true;
    }
    else if (layout == "IFD")
    {
        layoutFlat = false;
    }
    else
    {
        error("[config] unsupported flash layout config: {OPTION}", "OPTION",
              layout);
        info("supported options: 'Flat', 'IFD'");
        co_return false;
    }

    bool toolFlashrom;
    if (!optTool.has_value())
    {
        error("[config] error: no tool chose (property 'Tool')");
        co_return false;
    }

    const std::string& tool = optTool.value();

    if (tool == "flashrom")
    {
        toolFlashrom = true;
    }
    else if (tool == "None")
    {
        toolFlashrom = false;
    }
    else
    {
        error("[config] unsupported Tool: {OPTION}", "OPTION", tool);
        co_return false;
    }

    auto spiDevice = std::make_unique<SPIDevice>(
        ctx, spiPath, dryRun, hasME, gpioLines, gpioValues, config, this,
        layoutFlat, toolFlashrom);

    // we do not know the version on startup, it becomes known on update
    std::string version = "unknown";

    std::unique_ptr<Software> bsws =
        std::make_unique<Software>(ctx, *spiDevice);

    bsws->setVersion("unknown");

    // enable this software to be updated
    std::set<RequestedApplyTimes> allowedApplyTimes = {
        RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};

    bsws->enableUpdate(allowedApplyTimes);

    spiDevice->softwareCurrent = std::move(bsws);

    devices.insert({config.objectPath, std::move(spiDevice)});

    co_return true;
}
