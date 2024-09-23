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

SPIDeviceCodeUpdater::SPIDeviceCodeUpdater(sdbusplus::async::context& io,
                                           bool isDryRun, bool debug) :
    SoftwareManager(io, configTypeSPIDevice, isDryRun, debug)
{}

void SPIDeviceCodeUpdater::getInitialConfigurationSingleDevice(
    const std::string& service, const std::string& path, const size_t ngpios)
{
    std::optional<std::string> optSpiPath =
        SoftwareManager::dbusGetRequiredConfigurationProperty<std::string>(
            service, path, "Path");
    std::optional<bool> optHasME =
        SoftwareManager::dbusGetRequiredConfigurationProperty<bool>(
            service, path, "HasME");

    std::optional<std::string> optLayout =
        SoftwareManager::dbusGetRequiredConfigurationProperty<std::string>(
            service, path, "Layout");

    std::optional<std::string> optTool =
        SoftwareManager::dbusGetRequiredConfigurationProperty<std::string>(
            service, path, "Tool");

    std::optional<uint64_t> optVendorIANA =
        SoftwareManager::dbusGetRequiredConfigurationProperty<uint64_t>(
            service, path, "VendorIANA");

    std::optional<std::string> optCompatible =
        SoftwareManager::dbusGetRequiredConfigurationProperty<std::string>(
            service, path, "Compatible");

    if (!optSpiPath.has_value() || !optHasME.has_value() ||
        !optVendorIANA.has_value() || !optCompatible.has_value())
    {
        return;
    }

    std::string spiPath = optSpiPath.value();
    bool hasME = optHasME.value();

    lg2::debug("[config] spi device: {SPIDEV}", "SPIDEV", spiPath);

    std::vector<std::string> gpioLines;
    std::vector<uint8_t> gpioValues;

    for (size_t i = 0; i < ngpios; i++)
    {
        std::string intf =
            getConfigurationInterfaceMuxGpios() + std::to_string(i);
        std::optional<std::string> optGpioName =
            SoftwareManager::dbusGetRequiredProperty<std::string>(
                service, path, intf, "Name");
        std::optional<std::string> optGpioPolarity =
            SoftwareManager::dbusGetRequiredProperty<std::string>(
                service, path, intf, "Polarity");

        if (!optGpioName.has_value() || !optGpioPolarity.has_value())
        {
            return;
        }

        gpioLines.push_back(optGpioName.value());
        gpioValues.push_back((optGpioPolarity.value() == "High") ? 1 : 0);

        lg2::debug("[config] gpio {NAME} = {VALUE}", "NAME",
                   optGpioName.value(), "VALUE", optGpioPolarity.value());
    }

    lg2::debug("[config] extracted {N} gpios from EM config", "N", ngpios);

    uint32_t vendorIANA = 0;
    try
    {
        vendorIANA = optVendorIANA.value();
    }
    catch (std::exception& e)
    {
        lg2::error(e.what());
        return;
    }

    bool layoutFlat;

    if (!optLayout.has_value())
    {
        lg2::info("[config] error: no flash layout chosen (property 'Layout')");
        return;
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
        return;
    }

    bool toolFlashrom;
    if (!optTool.has_value())
    {
        lg2::error("[config] error: no tool chose (property 'Tool')");
        return;
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
        return;
    }

    auto spiDevice = std::make_unique<SPIDevice>(
        io, spiPath, dryRun, hasME, gpioLines, gpioValues, vendorIANA,
        optCompatible.value(), this, path, layoutFlat, toolFlashrom);

    std::unique_ptr<Software> bsws =
        std::make_unique<Software>(io, path.c_str(), dryRun, *spiDevice);

    // enable this software to be updated
    bsws->enableUpdate();

    spiDevice->softwareCurrent = std::move(bsws);

    devices.insert(std::move(spiDevice));
}

// NOLINTBEGIN
auto SPIDeviceCodeUpdater::getInitialConfiguration() -> sdbusplus::async::task<>
// NOLINTEND
{
    auto client = sdbusplus::client::xyz::openbmc_project::ObjectMapper<>(io)
                      .service("xyz.openbmc_project.ObjectMapper")
                      .path("/xyz/openbmc_project/object_mapper");

    auto res =
        co_await client.get_sub_tree("/xyz/openbmc_project/inventory", 0, {});

    lg2::debug("[config] looking for dbus interface {INTF}", "INTF",
               getConfigurationInterface());

    for (auto& [path, v] : res)
    {
        for (auto& [serviceName, interfaceNames] : v)
        {
            bool found = false;
            size_t ngpios = 0;
            for (std::string& interfaceName : interfaceNames)
            {
                // lg2::debug("dbus interface {INTF}", "INTF", interfaceName);
                if (interfaceName == getConfigurationInterface())
                {
                    found = true;
                }

                if (interfaceName.starts_with(
                        "xyz.openbmc_project.Configuration.SPIFlash.MuxGpios"))
                {
                    ngpios++;
                }
            }

            if (!found)
            {
                continue;
            }

            lg2::debug("[config] object path {PATH}", "PATH", path);
            lg2::debug("[config] service {SERVICE}", "SERVICE", serviceName);

            lg2::debug("[config] found spi flash configuration interface");

            getInitialConfigurationSingleDevice(serviceName, path, ngpios);
        }
    }

    lg2::debug("[config] done with initial configuration");

    requestBusName();
}
