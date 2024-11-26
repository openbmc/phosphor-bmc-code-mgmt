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
    SoftwareManager(ctx, configTypeBIOS), dryRun(isDryRun)
{}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDeviceCodeUpdater::initDevice(
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

    if (!spiControllerIndex.has_value() || !spiDeviceIndex.has_value())
    {
        co_return false;
    }

    std::optional<bool> hasME = co_await dbusGetRequiredProperty<bool>(
        ctx, service, path, configIface, "HasME");

    std::optional<std::string> layout =
        co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                      configIface, "Layout");

    std::optional<std::string> tool =
        co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                      configIface, "Tool");

    if (!hasME.has_value())
    {
        co_return false;
    }

    debug("[config] spi device: {INDEX1}:{INDEX2}", "INDEX1",
          spiControllerIndex.value(), "INDEX2", spiDeviceIndex.value());

    std::optional<std::vector<std::string>> configGPIONames =
        co_await dbusGetRequiredProperty<std::vector<std::string>>(
            ctx, service, path, configIface, "MuxOutputs");
    std::optional<std::vector<uint64_t>> configGPIOValues =
        co_await dbusGetRequiredProperty<std::vector<uint64_t>>(
            ctx, service, path, configIface, "MuxGPIOValues");

    if (!configGPIONames.has_value() || !configGPIOValues.has_value())
    {
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

    std::vector<std::string> gpioLines;
    std::vector<uint8_t> gpioValues;

    for (size_t i = 0; i < configGPIONames.value().size(); i++)
    {
        gpioLines.push_back(configGPIONames.value()[i]);
        gpioValues.push_back(configGPIOValues.value()[i]);

        debug("[config] gpio {NAME} = {VALUE}", "NAME",
              configGPIONames.value()[i], "VALUE", configGPIOValues.value()[i]);
    }

    debug("[config] extracted {N} gpios from EM config", "N", gpioLines.size());

    bool layoutFlat;

    if (!layout.has_value())
    {
        info("[config] error: no flash layout chosen (property 'Layout')");
        co_return false;
    }

    if (layout.value() == "Flat")
    {
        layoutFlat = true;
    }
    else if (layout.value() == "IFD")
    {
        layoutFlat = false;
    }
    else
    {
        error("[config] unsupported flash layout config: {OPTION}", "OPTION",
              layout.value());
        info("supported options: 'Flat', 'IFD'");
        co_return false;
    }

    bool toolFlashrom;
    if (!tool.has_value())
    {
        error("[config] error: no tool chose (property 'Tool')");
        co_return false;
    }

    if (tool.value() == "flashrom")
    {
        toolFlashrom = true;
    }
    else if (tool.value() == "None")
    {
        toolFlashrom = false;
    }
    else
    {
        error("[config] unsupported Tool: {OPTION}", "OPTION", tool.value());
        co_return false;
    }

    auto spiDevice = std::make_unique<SPIDevice>(
        ctx, spiControllerIndex.value(), spiDeviceIndex.value(), dryRun,
        hasME.value(), gpioLines, gpioValues, config, this, layoutFlat,
        toolFlashrom);

    // we do not know the version on startup, it becomes known on update
    std::string version = "unknown";

    std::unique_ptr<Software> bsws =
        std::make_unique<Software>(ctx, *spiDevice);

    bsws->setVersion(SPIDevice::readExternalVersion());

    // enable this software to be updated
    std::set<RequestedApplyTimes> allowedApplyTimes = {
        RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};

    bsws->enableUpdate(allowedApplyTimes);

    spiDevice->softwareCurrent = std::move(bsws);

    devices.insert({config.objectPath, std::move(spiDevice)});

    co_return true;
}
