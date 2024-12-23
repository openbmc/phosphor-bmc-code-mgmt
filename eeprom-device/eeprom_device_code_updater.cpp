#include "eeprom_device_code_updater.hpp"

#include "eeprom_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

PHOSPHOR_LOG2_USING;

template <typename T>
sdbusplus::async::task<std::optional<T>> dbusGetRequiredProperty(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& path, const std::string& intf,
    const std::string& property)
{
    auto client =
        sdbusplus::async::proxy().service(service).path(path).interface(
            "org.freedesktop.DBus.Properties");

    try
    {
        std::variant<T> result =
            co_await client.call<std::variant<T>>(ctx, "Get", intf, property);

        co_return std::get<T>(result);
    }
    catch (const std::exception& e)
    {
        error(
            "[config] missing property {PROPERTY} on path {PATH}, interface {INTF}",
            "PROPERTY", property, "PATH", path, "INTF", intf);
    }

    co_return std::nullopt;
}

EEPROMDeviceCodeUpdater::EEPROMDeviceCodeUpdater(
    sdbusplus::async::context& ctx) :
    SoftwareManager(ctx, configTypeEEPROMDevice)
{}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<> EEPROMDeviceCodeUpdater::initDevice(
    const std::string& service, const std::string& path, SoftwareConfig& config)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::string configIntf =
        "xyz.openbmc_project.Configuration." + config.configType;

    std::optional<uint64_t> optBus = co_await dbusGetRequiredProperty<uint64_t>(
        ctx, service, path, configIntf, "Bus");

    std::optional<uint64_t> optAddr =
        co_await dbusGetRequiredProperty<uint64_t>(ctx, service, path,
                                                   configIntf, "Address");

    if (!optBus.has_value() || !optAddr.has_value())
    {
        error("[config] error: missing required properties");
        co_return;
    }

    auto busNum = static_cast<uint8_t>(optBus.value());
    auto addr = static_cast<uint8_t>(optAddr.value());

    debug("[config] EEPROM device on Bus: {BUS} at Address: {ADDR}", "BUS",
          busNum, "ADDR", addr);

    std::vector<std::string> gpioLines;
    std::vector<uint8_t> gpioPolarities;

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
            gpioPolarities.push_back((polarity == "High") ? 1 : 0);
        }
    }

    if (gpioLines.size() != gpioPolarities.size())
    {
        error("[config] MuxGpios size mismatch: {GPIOS} vs {POLARITIES}",
              "GPIOS", gpioLines.size(), "POLARITIES", gpioPolarities.size());
        co_return;
    }

    for (size_t i = 0; i < gpioLines.size(); i++)
    {
        debug("[config] mux gpio {NAME} polarity = {VALUE}", "NAME",
              gpioLines[i], "VALUE", gpioPolarities[i]);
    }

    debug("[config] extracted {N} mux gpios from EM config", "N",
          gpioLines.size());

    auto eepromDevice = std::make_unique<EEPROMDevice>(
        ctx, busNum, addr, gpioLines, gpioPolarities, config, this);

    std::string version = "unknown";

    std::unique_ptr<Software> eepromsw =
        std::make_unique<Software>(ctx, *eepromDevice);

    eepromsw->setVersion(version);

    std::set<RequestedApplyTimes> allowedApplyTimes = {
        RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};

    eepromsw->enableUpdate(allowedApplyTimes);

    eepromDevice->softwareCurrent = std::move(eepromsw);

    devices.insert({config.objectPath, std::move(eepromDevice)});
}
