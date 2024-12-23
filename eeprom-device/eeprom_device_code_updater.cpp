#include "eeprom_device_code_updater.hpp"

#include "common/include/dbus_helper.hpp"
#include "eeprom_device.hpp"

#include <phosphor-logging/lg2.hpp>

EEPROMDeviceCodeUpdater::EEPROMDeviceCodeUpdater(sdbusplus::async::context& ctx,
                                                 bool dryRun) :
    SoftwareManager(ctx, configTypeEEPROMDevice), dryRun(dryRun)
{}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<> EEPROMDeviceCodeUpdater::initSingleDevice(
    const std::string& service, const std::string& path, SoftwareConfig& config)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::optional<uint64_t> optBusNo =
        co_await dbusGetRequiredConfigurationProperty<uint64_t>(
            ctx, service, path, "Bus", config);

    std::optional<uint64_t> optAddr =
        co_await dbusGetRequiredConfigurationProperty<uint64_t>(
            ctx, service, path, "Address", config);

    if (!optBusNo.has_value() || !optAddr.has_value())
    {
        lg2::error("[config] error: missing required properties");
        co_return;
    }

    auto busBo = static_cast<uint8_t>(optBusNo.value());
    auto addr = static_cast<uint8_t>(optAddr.value());

    lg2::debug("[config] EEPROM device on Bus: {BUS} at Address: {ADDR}", "BUS",
               busBo, "ADDR", addr);

    std::vector<MuxGpioConfig> muxGpioConfigs;
    std::string configIntfMuxGpios = "xyz.openbmc_project.Configuration." +
                                     configTypeEEPROMDevice + ".MuxGpios";

    for (size_t i = 0; true; i++)
    {
        std::string intf = configIntfMuxGpios + std::to_string(i);

        std::optional<std::string> optGpioName =
            co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                          intf, "Name");

        std::optional<std::string> optGpioPolarity =
            co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                          intf, "Polarity");

        std::optional<std::string> optGpioChipName =
            co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                          intf, "ChipName");

        if (!optGpioName.has_value() || !optGpioPolarity.has_value() ||
            !optGpioChipName.has_value())
        {
            break;
        }

        muxGpioConfigs.emplace_back(
            optGpioChipName.value(), optGpioName.value(),
            (optGpioPolarity.value() == "High") ? 1 : 0);

        lg2::debug("[config] chip {CHIP} gpio {NAME} = {VALUE}", "CHIP",
                   optGpioChipName.value(), "NAME", optGpioName.value(),
                   "VALUE", optGpioPolarity.value());
    }

    lg2::debug("[config] extracted {N} gpios from EM config", "N",
               muxGpioConfigs.size());

    auto eepromDevice = std::make_unique<EEPROMDevice>(
        ctx, dryRun, busBo, addr, muxGpioConfigs, config, this);

    std::string version = "unknown";

    std::unique_ptr<Software> eepromsw =
        std::make_unique<Software>(ctx, *eepromDevice);

    eepromsw->setVersion(version);

    // enable this software to be updated
    std::set<RequestedApplyTimes> allowedApplyTimes = {
        RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};

    eepromsw->enableUpdate(allowedApplyTimes);

    eepromDevice->softwareCurrent = std::move(eepromsw);

    devices.insert({config.objectPath, std::move(eepromDevice)});
}
