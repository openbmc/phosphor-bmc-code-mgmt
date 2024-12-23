#include "eeprom_device_software_manager.hpp"

#include "common/include/dbus_helper.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

#include <fstream>
#include <optional>
#include <sstream>

PHOSPHOR_LOG2_USING;

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> EEPROMDeviceSoftwareManager::getDeviceProperties(
    const std::string& service, const std::string& path,
    const std::string& intf, uint16_t& bus, uint8_t& address,
    std::string& chipModel)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::optional<uint64_t> optBus = co_await dbusGetRequiredProperty<uint64_t>(
        ctx, service, path, intf, "Bus");

    std::optional<uint64_t> optAddr =
        co_await dbusGetRequiredProperty<uint64_t>(ctx, service, path, intf,
                                                   "Address");

    std::optional<std::string> optChipModel =
        co_await dbusGetRequiredProperty<std::string>(ctx, service, path, intf,
                                                      "ChipModel");

    if (!optBus.has_value() || !optAddr.has_value() ||
        !optChipModel.has_value())
    {
        error("Missing properties on interface {INTF}", "INTF", intf);
        co_return false;
    }

    bus = static_cast<uint16_t>(optBus.value());
    address = static_cast<uint8_t>(optAddr.value());
    chipModel = optChipModel.value();

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> EEPROMDeviceSoftwareManager::initDevice(
    const std::string& service, const std::string& path, SoftwareConfig& config)
// NOLINTEND(readability-static-accessed-through-instance)
{
    const std::string configIface =
        "xyz.openbmc_project.Configuration." + config.configType;

    uint16_t bus;
    uint8_t addr;
    std::string chipModel;
    const std::string configIfaceDeviceInfo = configIface + ".DeviceInfo";

    if (!(co_await getDeviceProperties(service, path, configIfaceDeviceInfo,
                                       bus, addr, chipModel)))
    {
        co_return false;
    }

    debug("Device Info: Bus={BUS}, Address={ADDR}, ChipModel={CHIP}", "BUS",
          bus, "ADDR", addr, "CHIP", chipModel);

    std::unique_ptr<DeviceVersion> deviceVersion =
        getVersionProvider(chipModel, bus, addr);

    if (!deviceVersion)
    {
        error("Failed to get version provider for chip model: {CHIP}", "CHIP",
              chipModel);
        co_return false;
    }

    const std::string configIfaceEEPROMInfo = configIface + ".EEPROMInfo";

    if (!(co_await getDeviceProperties(service, path, configIfaceEEPROMInfo,
                                       bus, addr, chipModel)))
    {
        co_return false;
    }

    debug("EEPROM Info: Bus={BUS}, Address={ADDR}, ChipModel={CHIP}", "BUS",
          bus, "ADDR", addr, "CHIP", chipModel);

    const std::string configIfaceMux = configIface + ".MuxOutputs";
    std::vector<std::string> gpioLines;
    std::vector<bool> gpioPolarities;

    for (size_t i = 0; true; i++)
    {
        const std::string iface = configIfaceMux + std::to_string(i);

        std::optional<std::string> name =
            co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                          iface, "Name");

        std::optional<std::string> polarity =
            co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                          iface, "Polarity");

        if (!name.has_value() || !polarity.has_value())
        {
            break;
        }

        gpioLines.push_back(name.value());
        gpioPolarities.push_back(polarity.value() == "High");
    }

    for (size_t i = 0; i < gpioLines.size(); i++)
    {
        debug("Mux gpio {NAME} polarity = {VALUE}", "NAME", gpioLines[i],
              "VALUE", gpioPolarities[i]);
    }

    std::string version = deviceVersion->getVersion();

    auto eepromDevice = std::make_unique<EEPROMDevice>(
        ctx, bus, addr, chipModel, gpioLines, gpioPolarities,
        std::move(deviceVersion), config, this);

    std::unique_ptr<Software> software =
        std::make_unique<Software>(ctx, *eepromDevice);

    software->setVersion(version.empty() ? "Unknown" : version);

    std::set<RequestedApplyTimes> allowedApplyTimes = {
        RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};

    software->enableUpdate(allowedApplyTimes);

    eepromDevice->softwareCurrent = std::move(software);

    devices.insert({config.objectPath, std::move(eepromDevice)});

    co_return true;
}
