#include "pcieswitch_software_manager.hpp"

#include "common/include/dbus_helper.hpp"
#include "common/include/software_manager.hpp"
#include "spi_device.hpp"

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

using namespace phosphor::software;

PHOSPHOR_LOG2_USING;

sdbusplus::async::task<bool> PCIESWITCHSoftwareManager::initDevice(
    const std::string& service, const std::string& path,
    SoftwareConfig& config)
{ 
    std::string configIface =
        "xyz.openbmc_project.Configuration." + config.configType;

    auto chipName = co_await dbusGetRequiredProperty<std::string>(
        ctx, service, path, configIface, "Name");
    auto busNo = co_await dbusGetRequiredProperty<uint64_t>(
        ctx, service, path, configIface, "Bus");
    auto address = co_await dbusGetRequiredProperty<uint64_t>(
        ctx, service, path, configIface, "Address");

    if (!chipName.has_value() || !busNo.has_value() ||
        !address.has_value())
    {
        error("Missing property: Name, Bus, or Address");
        co_return false;
    }

    std::vector<std::string> gpioLines;
    std::vector<std::string> gpioValues;

    const std::string configIfaceMux = configIface + ".MuxOutputs";
    for (size_t i = 0; true; i++)
    {
        const std::string iface = configIfaceMux + std::to_string(i);
    
        std::optional<std::string> name =
            co_await dbusGetRequiredProperty<std::string>(ctx, 
                                service, path, iface, "Name");
        std::optional<std::string> polarity =
            co_await dbusGetRequiredProperty<std::string>(ctx,
                                service, path, iface, "Polarity");
    
        if (!name.has_value() || !polarity.has_value())
        {
            break;
        }
    
        gpioLines.push_back(name.value());
        gpioValues.push_back((polarity == "High") ? "1" : "0");
        info("MuxOutput{NUM} name: {NAME}, value: {VAL}", 
             "NUM", std::to_string(i),
             "NAME", gpioLines[i], "VAL", gpioValues[i]);
    }

    std::unique_ptr<SPIDevice> spiDevice;
    try
    {
        spiDevice = std::make_unique<SPIDevice>(ctx, 
            chipName.value(), gpioLines, gpioValues, config, this);
    }
    catch (std::exception& e)
    {
        co_return false;
    }

    std::unique_ptr<Software> software =
        std::make_unique<Software>(ctx, *spiDevice);

    std::set<RequestedApplyTimes> allowedApplyTimes = {
        RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};

    software->enableUpdate(allowedApplyTimes);

    spiDevice->softwareCurrent = std::move(software);

    std::string version = "unknown";
    if (!(co_await spiDevice->getVersion(version)))
    {
        lg2::error("Failed to get PCIe switch version for {NAME}", "NAME",
                   chipName.value());
    }

    spiDevice->softwareCurrent->setVersion(
        version, SoftwareVersion::VersionPurpose::Other);

    devices.insert({config.objectPath, std::move(spiDevice)});

    co_return true;
}

int main()
{
    sdbusplus::async::context ctx;

    PCIESWITCHSoftwareManager pcieSwitchSoftwareManager(ctx);

    std::vector<std::string> configIntfs = {
        "xyz.openbmc_project.Configuration.BRCM_PEX90080Firmware",
    };

    ctx.spawn(pcieSwitchSoftwareManager.initDevices(configIntfs));

    ctx.run();

    return 0;
}

