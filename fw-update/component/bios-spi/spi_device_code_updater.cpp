#include "spi_device_code_updater.hpp"

#include "fw-update/common/include/software_manager.hpp"
#include "spi_device.hpp"

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

// 'Type' field in EM exposes record
const std::string configTypeSPIDevice = "SPIFlash";

SPIDeviceCodeUpdater::SPIDeviceCodeUpdater(sdbusplus::async::context& ctx,
                                           bool isDryRun, bool debug) :
    SoftwareManager(ctx, configTypeSPIDevice, isDryRun), debug(debug)
{}

// NOLINTBEGIN
sdbusplus::async::task<>
    SPIDeviceCodeUpdater::getInitialConfigurationSingleDevice(
        const std::string& service, DeviceConfig& config)
// NOLINTEND
{
    const std::string& path = config.objPathConfig;

    std::optional<std::string> optSpiPath =
        co_await SoftwareManager::dbusGetRequiredConfigurationProperty<
            std::string>(service, path, "Path");

    std::optional<bool> optHasME =
        co_await SoftwareManager::dbusGetRequiredConfigurationProperty<bool>(
            service, path, "HasME");

    std::optional<std::string> optLayout =
        co_await SoftwareManager::dbusGetRequiredConfigurationProperty<
            std::string>(service, path, "Layout");

    std::optional<std::string> optTool =
        co_await SoftwareManager::dbusGetRequiredConfigurationProperty<
            std::string>(service, path, "Tool");

    if (!optSpiPath.has_value() || !optHasME.has_value())
    {
        co_return;
    }

    std::string spiPath = optSpiPath.value();
    bool hasME = optHasME.value();

    lg2::debug("[config] spi device: {SPIDEV}", "SPIDEV", spiPath);

    std::vector<std::string> gpioLines;
    std::vector<uint8_t> gpioValues;

    for (size_t i = 0; true; i++)
    {
        std::string intf =
            getConfigurationInterfaceMuxGpios() + std::to_string(i);
        std::optional<std::string> optGpioName =
            co_await SoftwareManager::dbusGetRequiredProperty<std::string>(
                service, path, intf, "Name");
        std::optional<std::string> optGpioPolarity =
            co_await SoftwareManager::dbusGetRequiredProperty<std::string>(
                service, path, intf, "Polarity");

        if (!optGpioName.has_value() || !optGpioPolarity.has_value())
        {
            break;
        }

        gpioLines.push_back(optGpioName.value());
        gpioValues.push_back((optGpioPolarity.value() == "High") ? 1 : 0);

        lg2::debug("[config] gpio {NAME} = {VALUE}", "NAME",
                   optGpioName.value(), "VALUE", optGpioPolarity.value());
    }

    lg2::debug("[config] extracted {N} gpios from EM config", "N",
               gpioLines.size());

    bool layoutFlat;

    if (!optLayout.has_value())
    {
        lg2::info("[config] error: no flash layout chosen (property 'Layout')");
        co_return;
    }

    const std::string layout = optLayout.value();
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
        lg2::error("[config] unsupported flash layout config: {OPTION}",
                   "OPTION", layout);
        lg2::info("supported options: 'Flat', 'IFD'");
        co_return;
    }

    bool toolFlashrom;
    if (!optTool.has_value())
    {
        lg2::error("[config] error: no tool chose (property 'Tool')");
        co_return;
    }

    const std::string tool = optTool.value();

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
        lg2::error("[config] unsupported Tool: {OPTION}", "OPTION", tool);
        co_return;
    }

    auto spiDevice = std::make_unique<SPIDevice>(
        ctx, spiPath, dryRun, hasME, gpioLines, gpioValues, config, this,
        layoutFlat, toolFlashrom, this->debug);

    std::unique_ptr<Software> bsws =
        std::make_unique<Software>(ctx, path.c_str(), *spiDevice);

    // enable this software to be updated
    std::set<RequestedApplyTimes> allowedApplyTimes = {
        RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};

    bsws->enableUpdate(allowedApplyTimes);

    spiDevice->softwareCurrent = std::move(bsws);

    devices.insert(std::move(spiDevice));
}
