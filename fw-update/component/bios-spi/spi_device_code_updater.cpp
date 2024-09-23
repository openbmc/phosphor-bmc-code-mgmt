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

void SPIDeviceCodeUpdater::getInitialConfigurationSingleDevice(
    const std::string& service, const std::string& path)
{
    auto m2 = bus.new_method_call(service.c_str(), path.c_str(),
                                  "org.freedesktop.DBus.Properties", "Get");
    m2.append("xyz.openbmc_project.Configuration.SPIFlash", "Path");

    auto reply = m2.call();

    std::variant<std::string> vpath;
    reply.read(vpath);

    std::string spiPath = std::get<std::string>(vpath);

    lg2::debug("spi device: {SPIDEV}", "SPIDEV", spiPath);

    // I don't see how find the softwareid from device here.
    // With SPI flash, the chip might
    // be muxed to the host and not accessible to the BMC.
    std::string swid = "dbus_swid_version_" + getRandomSoftwareId();

    std::string objPathStr = getObjPath(swid);
    const char* objPath = objPathStr.c_str();

    auto spiDevice = std::make_shared<SPIDevice>(io, serviceName, dryRun, this);

    std::shared_ptr<BiosSW> bsws =
        std::make_shared<BiosSW>(bus, swid, objPath, &(*spiDevice));

    spiDevice->softwareCurrent = bsws;

    devices.insert(spiDevice);
}

auto SPIDeviceCodeUpdater::getInitialConfiguration() -> sdbusplus::async::task<>
{
    // auto client =
    // sdbusplus::async::client_t<sdbusplus::client::xyz::openbmc_project::ObjectMapper>(io)
    auto client = sdbusplus::client::xyz::openbmc_project::ObjectMapper<>(io)
                      .service("xyz.openbmc_project.ObjectMapper")
                      .path("/xyz/openbmc_project/object_mapper");

    auto res =
        co_await client.get_sub_tree("/xyz/openbmc_project/inventory", 0, {});

    std::string spiFlashInterface =
        "xyz.openbmc_project.Configuration.SPIFlash";

    lg2::debug("looking for dbus interface {INTF}", "INTF", spiFlashInterface);

    for (auto& [path, v] : res)
    {
        for (auto& [serviceName, interfaceNames] : v)
        {
            for (std::string& interfaceName : interfaceNames)
            {
                // lg2::debug("dbus interface {INTF}", "INTF", interfaceName);
                if (interfaceName != spiFlashInterface)
                {
                    continue;
                }

                lg2::debug("object path {PATH}", "PATH", path);
                lg2::debug("service {SERVICE}", "SERVICE", serviceName);

                lg2::debug("found spi flash configuration interface");

                getInitialConfigurationSingleDevice(serviceName, path);
            }
        }
    }

    lg2::debug("DONE looking for dbus interface {INTF}", "INTF",
               spiFlashInterface);
}

bool SPIDeviceCodeUpdater::verifyImage(sdbusplus::message::unix_fd image)
{
    (void)image;
    // TODO(design: verify image
    lg2::info("spi device code updater - verifying image...");
    return true;
}
