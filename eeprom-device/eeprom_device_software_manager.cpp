#include "eeprom_device_software_manager.hpp"

#include "common/include/dbus_helper.hpp"
#include "eeprom_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

#include <fstream>
#include <optional>
#include <sstream>

PHOSPHOR_LOG2_USING;

namespace SoftwareInf = phosphor::software;

void EEPROMDeviceSoftwareManager::start()
{
    ctx.spawn(initDevices());
    ctx.run();
}

bool EEPROMDeviceSoftwareManager::isSupported(const std::string& configType)
{
    return ::isSupported(configType);
}

sdbusplus::async::task<bool> EEPROMDeviceSoftwareManager::initDevice(
    const std::string& service, const sdbusplus::object_path& path,
    SoftwareConfig& config)
{
    auto bus = config.getProperty<uint64_t>("Bus");
    auto address = config.getProperty<uint64_t>("Address");
    auto fwDevice = config.getProperty<std::string>("FirmwareDevice");

    if (!bus.has_value() || !address.has_value() || !fwDevice.has_value())
    {
        error("Missing EEPROM device config property");
        co_return false;
    }

    debug("EEPROM Device: Bus={BUS}, Address={ADDR}, Type={TYPE}, "
          "Firmware Device={DEVICE}",
          "BUS", bus.value(), "ADDR", address.value(), "TYPE",
          config.configType, "DEVICE", fwDevice.value());

    std::unique_ptr<DeviceVersion> deviceVersion =
        getVersionProvider(config.configType, bus.value(), address.value());

    if (!deviceVersion)
    {
        error("Failed to get version provider for chip type: {CHIP}", "CHIP",
              config.configType);
        co_return false;
    }

    std::string version = deviceVersion->getVersion();

    using ObjectMapper =
        sdbusplus::client::xyz::openbmc_project::ObjectMapper<>;

    auto mapper = ObjectMapper(ctx)
                      .service(ObjectMapper::default_service)
                      .path(ObjectMapper::instance_path);

    auto res =
        co_await mapper.get_sub_tree("/xyz/openbmc_project/inventory", 0, {});

    bus.reset();
    address.reset();
    std::optional<std::string> type;

    for (auto& [p, v] : res)
    {
        if (!p.ends_with(fwDevice.value()))
        {
            continue;
        }

        for (auto& [s, ifaces] : v)
        {
            for (std::string& iface : ifaces)
            {
                if (iface.starts_with("xyz.openbmc_project.Configuration."))
                {
                    bus = co_await dbusGetRequiredProperty<uint64_t>(
                        ctx, s, p, iface, "Bus");

                    address = co_await dbusGetRequiredProperty<uint64_t>(
                        ctx, s, p, iface, "Address");

                    type = co_await dbusGetRequiredProperty<std::string>(
                        ctx, s, p, iface, "Type");
                    break;
                }
            }
            if (bus.has_value() && address.has_value() && type.has_value())
            {
                break;
            }
        }
        break;
    }

    if (!bus.has_value() || !address.has_value() || !type.has_value())
    {
        error("Missing EEPROM config property");
        co_return false;
    }

    debug("EEPROM: Bus={BUS}, Address={ADDR}, Type={TYPE}", "BUS", bus.value(),
          "ADDR", address.value(), "TYPE", type.value());

    const std::string configIfaceMux = config.baseInterface + ".MuxOutputs";
    std::vector<std::string> gpioLines;
    std::vector<bool> gpioPolarities;

    for (size_t i = 0; true; i++)
    {
        const std::string iface = configIfaceMux + std::to_string(i);

        auto name = config.getProperty<std::string>(iface, "Name");
        if (!name.has_value())
        {
            name = co_await dbusGetRequiredProperty<std::string>(
                ctx, service, path.str, iface, "Name");
        }

        auto polarity = config.getProperty<std::string>(iface, "Polarity");
        if (!polarity.has_value())
        {
            polarity = co_await dbusGetRequiredProperty<std::string>(
                ctx, service, path.str, iface, "Polarity");
        }

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

    auto eepromDevice = std::make_unique<EEPROMDevice>(
        ctx, static_cast<uint16_t>(bus.value()),
        static_cast<uint8_t>(address.value()), type.value(), gpioLines,
        gpioPolarities, std::move(deviceVersion), config, this);

    std::unique_ptr<SoftwareInf::Software> software =
        std::make_unique<SoftwareInf::Software>(ctx, *eepromDevice);

    software->setVersion(version.empty() ? "Unknown" : version,
                         SoftwareInf::SoftwareVersion::VersionPurpose::Other);

    std::set<RequestedApplyTimes> allowedApplyTimes = {
        RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};

    software->enableUpdate(allowedApplyTimes);

    eepromDevice->softwareCurrent = std::move(software);

    devices.insert({config.objectPath, std::move(eepromDevice)});

    co_return true;
}

int main()
{
    sdbusplus::async::context ctx;

    EEPROMDeviceSoftwareManager eepromDeviceSoftwareManager(ctx);

    eepromDeviceSoftwareManager.start();
    return 0;
}
