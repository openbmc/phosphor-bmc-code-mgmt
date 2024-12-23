#include "eeprom_device_code_updater.hpp"

#include "common/include/dbus_helper.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

#include <fstream>
#include <optional>
#include <sstream>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::eeprom
{

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> EEPROMDeviceSoftwareManager::getDeviceProperties(
    const std::string& service, const std::string& path,
    const std::string& intf, uint8_t& bus, uint8_t& address,
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
        error(
            "Failed to find required DeviceInfo properties on interface {INTF}",
            "INTF", intf);
        co_return false;
    }

    bus = static_cast<uint8_t>(optBus.value());
    address = static_cast<uint8_t>(optAddr.value());
    chipModel = optChipModel.value();

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> EEPROMDeviceSoftwareManager::initDevice(
    const std::string& service, const std::string& path, SoftwareConfig& config)
// NOLINTEND(readability-static-accessed-through-instance)
{
    const std::string configIntf =
        "xyz.openbmc_project.Configuration." + config.configType;
    const std::string deviceInfoIntf = configIntf + ".DeviceInfo";
    const std::string eepromInfoIntf = configIntf + ".EEPROMInfo";

    uint8_t bus;
    uint8_t addr;
    std::string chipModel;

    if (!(co_await getDeviceProperties(service, path, deviceInfoIntf, bus, addr,
                                       chipModel)))
    {
        co_return false;
    }

    debug("EEPROM device Info: Bus={BUS}, Address={ADDR}, ChipModel={CHIP}",
          "BUS", bus, "ADDR", addr, "CHIP", chipModel);

    std::unique_ptr<DeviceVersion> deviceVersion =
        getVersionProvider(chipModel, bus, addr);

    if (!deviceVersion)
    {
        error("Failed to get version provider for chip model: {CHIP}", "CHIP",
              chipModel);
        co_return false;
    }

    if (!(co_await getDeviceProperties(service, path, eepromInfoIntf, bus, addr,
                                       chipModel)))
    {
        co_return false;
    }

    debug("EEPROM Info: Bus={BUS}, Address={ADDR}, ChipModel={CHIP}", "BUS",
          bus, "ADDR", addr, "CHIP", chipModel);

    std::vector<std::string> gpioLines;
    std::vector<int> gpioPolarities;

    std::optional<std::vector<std::string>> optMuxGpios =
        co_await dbusGetRequiredProperty<std::vector<std::string>>(
            ctx, service, path, configIntf, "MuxGpios");

    std::optional<std::vector<std::string>> optMuxGPIOPolarities =
        co_await dbusGetRequiredProperty<std::vector<std::string>>(
            ctx, service, path, configIntf, "MuxGPIOPolarities");

    if (optMuxGpios.has_value())
    {
        gpioLines = std::move(optMuxGpios.value());
    }

    if (optMuxGPIOPolarities.has_value())
    {
        for (const auto& polarity : optMuxGPIOPolarities.value())
        {
            gpioPolarities.push_back((polarity == "ActiveHigh") ? 1 : 0);
        }
    }

    if (gpioLines.size() != gpioPolarities.size())
    {
        error("MuxGpios size mismatch: {GPIOS} vs {POLARITIES}", "GPIOS",
              gpioLines.size(), "POLARITIES", gpioPolarities.size());
        co_return false;
    }

    for (size_t i = 0; i < gpioLines.size(); i++)
    {
        debug("Mux gpio {NAME} polarity = {VALUE}", "NAME", gpioLines[i],
              "VALUE", gpioPolarities[i]);
    }

    debug("Extracted {N} mux gpios from EM config", "N", gpioLines.size());

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

} // namespace phosphor::software::eeprom
