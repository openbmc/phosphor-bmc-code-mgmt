#include "spi_device_code_updater.hpp"

#include "../../common/fw_manager.hpp"
#include "spi_device.hpp"
#include "spi_device_sw.hpp"

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

SPIDeviceCodeUpdater::SPIDeviceCodeUpdater(sdbusplus::async::context& io,
                                           bool isDryRun) :
    FWManager(io, "BIOS", isDryRun)
{}

std::string SPIDeviceCodeUpdater::getConfigurationInterface()
{
    return "xyz.openbmc_project.Configuration.SPIFlash";
}

void SPIDeviceCodeUpdater::getInitialConfigurationSingleDevice(
    const std::string& service, const std::string& path, const size_t ngpios)
{
    std::optional<std::string> optSpiPath =
        FWManager::dbusGetRequiredConfigurationProperty<std::string>(
            service, path, "Path");
    std::optional<bool> optHasME =
        FWManager::dbusGetRequiredConfigurationProperty<bool>(
            service, path, "HasME");

    std::optional<uint64_t> optVendorIANA =
        FWManager::dbusGetRequiredConfigurationProperty<uint64_t>(
            service, path, "VendorIANA");

    std::optional<std::string> optCompatible =
        FWManager::dbusGetRequiredConfigurationProperty<std::string>(
            service, path, "Compatible");

    if (!optSpiPath.has_value() || !optHasME.has_value() ||
        !optVendorIANA.has_value() || !optCompatible.has_value())
    {
        return;
    }

    std::string spiPath = optSpiPath.value();
    bool hasME = optHasME.value();

    lg2::debug("[config] spi device: {SPIDEV}", "SPIDEV", spiPath);

    std::string swid = getRandomSoftwareId();

    std::string objPathStr = getObjPathFromSwid(swid);

    std::vector<std::string> gpioLines;
    std::vector<uint8_t> gpioValues;

    for (size_t i = 0; i < ngpios; i++)
    {
        std::string intf =
            getConfigurationInterfaceMuxGpios() + std::to_string(i);
        std::optional<std::string> optGpioName =
            FWManager::dbusGetRequiredProperty<std::string>(service, path, intf,
                                                            "Name");
        std::optional<std::string> optGpioPolarity =
            FWManager::dbusGetRequiredProperty<std::string>(service, path, intf,
                                                            "Polarity");

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

    auto spiDevice = std::make_shared<SPIDevice>(
        io, spiPath, dryRun, hasME, gpioLines, gpioValues, vendorIANA,
        optCompatible.value(), this);

    std::shared_ptr<BiosSW> bsws = std::make_shared<BiosSW>(
        io, bus, swid, objPathStr.c_str(), &(*spiDevice));

    spiDevice->softwareCurrent = bsws;

    devices.insert(spiDevice);

    // TODO(design): Create functional association from Version to Inventory
    // Item
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
