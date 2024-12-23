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

const std::vector<std::string> emConfigTypes = {"PT5161L"};

void EEPROMDeviceSoftwareManager::start()
{
    std::vector<std::string> configIntfs;
    configIntfs.reserve(emConfigTypes.size());

    std::transform(emConfigTypes.begin(), emConfigTypes.end(),
                   std::back_inserter(configIntfs),
                   [](const std::string& type) {
                       return "xyz.openbmc_project.Configuration." + type;
                   });

    ctx.spawn(initDevices(configIntfs));
    ctx.run();
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> EEPROMDeviceSoftwareManager::initDevice(
    const std::string& service, const std::string& path, SoftwareConfig& config)
// NOLINTEND(readability-static-accessed-through-instance)
{
    const std::string configIface =
        "xyz.openbmc_project.Configuration." + config.configType;

    std::optional<uint64_t> bus = co_await dbusGetRequiredProperty<uint64_t>(
        ctx, service, path, configIface, "Bus");

    std::optional<uint64_t> address =
        co_await dbusGetRequiredProperty<uint64_t>(ctx, service, path,
                                                   configIface, "Address");

    std::optional<std::string> type =
        co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                      configIface, "Type");

    std::optional<std::string> fwDevice =
        co_await dbusGetRequiredProperty<std::string>(
            ctx, service, path, configIface, "FirmwareDevice");

    if (!bus.has_value() || !address.has_value() || !type.has_value() ||
        !fwDevice.has_value())
    {
        error("Missing EEPROM device config property");
        co_return false;
    }

    debug("EEPROM Device: Bus={BUS}, Address={ADDR}, Type={TYPE}, "
          "Firmware Device={DEVICE}",
          "BUS", bus.value(), "ADDR", address.value(), "TYPE", type.value(),
          "DEVICE", fwDevice.value());

    std::unique_ptr<DeviceVersion> deviceVersion =
        getVersionProvider(type.value(), bus.value(), address.value());

    if (!deviceVersion)
    {
        error("Failed to get version provider for chip type: {CHIP}", "CHIP",
              type.value());
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
    type.reset();

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

    auto eepromDevice = std::make_unique<EEPROMDevice>(
        ctx, static_cast<uint16_t>(bus.value()),
        static_cast<uint8_t>(address.value()), type.value(), gpioLines,
        gpioPolarities, std::move(deviceVersion), config, this);

    std::unique_ptr<SoftwareInf::Software> software =
        std::make_unique<SoftwareInf::Software>(ctx, *eepromDevice);

    software->setVersion(version.empty() ? "Unknown" : version);

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
