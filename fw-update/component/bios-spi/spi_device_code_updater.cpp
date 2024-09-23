#include "spi_device_code_updater.hpp"

#include "bios_sw.hpp"
#include "spi_device.hpp"

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

#include <fstream>

SPIDeviceCodeUpdater::SPIDeviceCodeUpdater(sdbusplus::async::context& io,
                                           bool isDryRun) :
    FWManager(io, "BIOS", isDryRun)
{
    // TODO(design): get device access info from
    // xyz/openbmc_project/inventory/system/... path

    // Swid = <DeviceX>_<RandomId>

    //(design): Create Interface xyz.openbmc_project.Software.Update at
    /// xyz/openbmc_project/Software/<SwId> (design): Create
    // Interfacexyz.openbmc_project.Software.Activation at
    /// xyz/openbmc_project/Software/<SwId>
    // with Status = Active

    // TODO(design): Create functional association from Version to Inventory
    // Item
}

template <typename T>
std::optional<T> SPIDeviceCodeUpdater::dbusGetProperty(
    const std::string& service, const std::string& path,
    const std::string& intf, const std::string& property)
{
    auto m = bus.new_method_call(service.c_str(), path.c_str(),
                                 "org.freedesktop.DBus.Properties", "Get");
    m.append(intf.c_str(), property.c_str());

    try
    {
        auto reply = m.call();

        std::variant<T> vpath;
        reply.read(vpath);

        T res = std::get<T>(vpath);
        return res;
    }
    catch (std::exception& e)
    {
        return std::nullopt;
    }
}

void SPIDeviceCodeUpdater::getInitialConfigurationSingleDevice(
    const std::string& service, const std::string& path, const size_t ngpios)
{
    const char* SPI_FLASH_INTF = "xyz.openbmc_project.Configuration.SPIFlash";
    const char* SPI_FLASH_GPIO_INTF =
        "xyz.openbmc_project.Configuration.SPIFlash.MuxGpios";

    std::optional<std::string> optSpiPath =
        dbusGetProperty<std::string>(service, path, SPI_FLASH_INTF, "Path");
    std::optional<bool> optHasME =
        dbusGetProperty<bool>(service, path, SPI_FLASH_INTF, "HasME");

    if (!optSpiPath.has_value())
    {
        lg2::error(
            "[config] property \"Path\" was absent, could not configure SPI Device");
        return;
    }
    if (!optHasME.has_value())
    {
        lg2::error(
            "[config] property \"HasME\" was absent, could not configure SPI Device");
        return;
    }

    std::string spiPath = optSpiPath.value();
    bool hasME = optHasME.value();

    lg2::debug("[config] spi device: {SPIDEV}", "SPIDEV", spiPath);

    // I don't see how find the softwareid from device here.
    // With SPI flash, the chip might
    // be muxed to the host and not accessible to the BMC.
    std::string swid = "dbus_swid_version_" + getRandomSoftwareId();

    std::string objPathStr = getObjPath(swid);

    std::vector<std::string> gpioLines;
    std::vector<uint8_t> gpioValues;

    for (size_t i = 0; i < ngpios; i++)
    {
        std::string intf = std::string(SPI_FLASH_GPIO_INTF) + std::to_string(i);
        std::optional<std::string> optGpioName =
            dbusGetProperty<std::string>(service, path, intf, "Name");
        std::optional<std::string> optGpioPolarity =
            dbusGetProperty<std::string>(service, path, intf, "Polarity");

        if (!optGpioName.has_value())
        {
            lg2::error(
                "[config] gpio {N} : property \"Name\" was absent, could not configure SPI Device",
                "N", i);
            return;
        }
        if (!optGpioPolarity.has_value())
        {
            lg2::error(
                "[config] gpio {N} : property \"Polarity\" was absent, could not configure SPI Device",
                "N", i);
            return;
        }

        gpioLines.push_back(optGpioName.value());
        gpioValues.push_back((optGpioPolarity.value() == "High") ? 1 : 0);

        lg2::debug("[config] gpio {NAME} = {VALUE}", "NAME",
                   optGpioName.value(), "VALUE", optGpioPolarity.value());
    }

    lg2::debug("[config] extracted {N} gpios from EM config", "N", ngpios);

    auto spiDevice = std::make_shared<SPIDevice>(io, serviceName, dryRun, hasME,
                                                 gpioLines, gpioValues, this);

    std::shared_ptr<BiosSW> bsws =
        std::make_shared<BiosSW>(bus, swid, objPathStr.c_str(), &(*spiDevice));

    spiDevice->softwareCurrent = bsws;

    devices.insert(spiDevice);
}

auto SPIDeviceCodeUpdater::getInitialConfiguration() -> sdbusplus::async::task<>
{
    auto client = sdbusplus::client::xyz::openbmc_project::ObjectMapper<>(io)
                      .service("xyz.openbmc_project.ObjectMapper")
                      .path("/xyz/openbmc_project/object_mapper");

    auto res =
        co_await client.get_sub_tree("/xyz/openbmc_project/inventory", 0, {});

    std::string spiFlashInterface =
        "xyz.openbmc_project.Configuration.SPIFlash";

    lg2::debug("[config] looking for dbus interface {INTF}", "INTF",
               spiFlashInterface);

    for (auto& [path, v] : res)
    {
        for (auto& [serviceName, interfaceNames] : v)
        {
            bool found = false;
            size_t ngpios = 0;
            for (std::string& interfaceName : interfaceNames)
            {
                // lg2::debug("dbus interface {INTF}", "INTF", interfaceName);
                if (interfaceName == spiFlashInterface)
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
}

bool SPIDeviceCodeUpdater::verifyImage(sdbusplus::message::unix_fd image)
{
    (void)image;
    // TODO(design: verify image
    lg2::info("spi device code updater - verifying image...");
    return true;
}
