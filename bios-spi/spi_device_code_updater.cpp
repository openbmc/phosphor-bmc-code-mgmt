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
sdbusplus::async::task<> SPIDeviceCodeUpdater::initDevice(
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
        co_return;
    }

    std::string spiPath = optSpiPath.value();
    bool hasME = optHasME.value();

    debug("[config] spi device: {SPIDEV}", "SPIDEV", spiPath);

    std::vector<std::string> gpioLines;
    std::vector<uint8_t> gpioValues;

    std::string configIntfMuxGpios;

    std::vector<std::string> configIntfs = {
        "xyz.openbmc_project.Configuration." + configTypeSPIDevice};

    for (auto& iface : configIntfs)
    {
        configIntfMuxGpios = iface + ".MuxGpios";
    }

    for (size_t i = 0; true; i++)
    {
        std::string intf = configIntfMuxGpios + std::to_string(i);
        std::optional<std::string> optGpioName =
            co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                          intf, "Name");
        std::optional<std::string> optGpioPolarity =
            co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                          intf, "Polarity");

        if (!optGpioName.has_value() || !optGpioPolarity.has_value())
        {
            break;
        }

        gpioLines.push_back(optGpioName.value());
        gpioValues.push_back((optGpioPolarity.value() == "High") ? 1 : 0);

        debug("[config] gpio {NAME} = {VALUE}", "NAME", optGpioName.value(),
              "VALUE", optGpioPolarity.value());
    }

    debug("[config] extracted {N} gpios from EM config", "N", gpioLines.size());

    bool layoutFlat;

    if (!optLayout.has_value())
    {
        info("[config] error: no flash layout chosen (property 'Layout')");
        co_return;
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
        co_return;
    }

    bool toolFlashrom;
    if (!optTool.has_value())
    {
        error("[config] error: no tool chose (property 'Tool')");
        co_return;
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
        co_return;
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
}
