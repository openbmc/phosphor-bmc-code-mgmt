#include "eeprom_device_code_updater.hpp"

#include "eeprom_device.hpp"

#include <phosphor-logging/lg2.hpp>

const std::string configTypeEEPROMDevice = "EEPROM";

EEPROMDeviceCodeUpdater::EEPROMDeviceCodeUpdater(
    sdbusplus::async::context& ctx, bool isDryRun) :
    SoftwareManager(ctx, configTypeEEPROMDevice, isDryRun)
{}

std::set<std::string> EEPROMDeviceCodeUpdater::getEMConfigTypes()
{
    return {configTypeEEPROMDevice};
}
// NOLINTBEGIN
sdbusplus::async::task<> 
    EEPROMDeviceCodeUpdater::getInitialConfigurationSingleDevice(
        const std::string& service, DeviceConfig& config)
// NOLINTEND
{
    const std::string& path = config.objPathConfig;

    std::optional<uint64_t> optBusNo =
        co_await SoftwareManager::dbusGetRequiredConfigurationProperty<
            uint64_t>(service, path, "Bus", config);
    
    std::optional<uint64_t> optAddr =
        co_await SoftwareManager::dbusGetRequiredConfigurationProperty<
            uint64_t>(service, path, "Address", config);

    if (!optBusNo.has_value() || !optAddr.has_value())
    {
        lg2::error("[config] error: missing required property");
        co_return;
    }

    auto busBo = static_cast<uint8_t>(optBusNo.value());
    auto addr = static_cast<uint8_t>(optAddr.value());

    lg2::debug(
        "[config] eeprom device on Bus: {BUS} at Address: {ADDR}",
        "BUS", busBo, "ADDR", addr);

    std::vector<MuxGpioConfig> muxGpioConfigs;

    for (size_t i = 0; true; i++)
    {
        std::string intf =
            getConfigurationInterfaceMuxGpios() + std::to_string(i);

        std::optional<std::string> optGpioName =
            co_await SoftwareManager::dbusGetRequiredProperty<std::string>(
                service, path, intf, "Name");

        std::optional<std::string> optGpioPolarity =
            co_await SoftwareManager::dbusGetRequiredProperty<std::string>(
                service, path, intf, "Polarity");

        std::optional<std::string> optGpioChipName =
            co_await SoftwareManager::dbusGetRequiredProperty<std::string>(
                service, path, intf, "ChipName");

        if (!optGpioName.has_value() || !optGpioPolarity.has_value() || 
            !optGpioChipName.has_value())
        {
            break;
        }

        muxGpioConfigs.emplace_back(optGpioChipName.value(), 
            optGpioName.value(), (optGpioPolarity.value() == "High") ? 1 : 0);

        lg2::debug("[config] chip {CHIP} gpio {NAME} = {VALUE}", 
                   "CHIP", optGpioChipName.value(), 
                   "NAME", optGpioName.value(), 
                   "VALUE", optGpioPolarity.value());
    }

    lg2::debug("[config] extracted {N} gpios from EM config", "N",
               muxGpioConfigs.size());

    auto eepromDevice = std::make_unique<EEPROMDevice>(
        ctx, dryRun, busBo, addr, muxGpioConfigs, config, this);

    std::string version = "unknown";

    std::unique_ptr<Software> eepromfw =
        std::make_unique<Software>(ctx, *eepromDevice);

    eepromfw->setVersion(version);

    // enable this software to be updated
    std::set<RequestedApplyTimes> allowedApplyTimes = {
        RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};

    eepromfw->enableUpdate(allowedApplyTimes);

    eepromDevice->softwareCurrent = std::move(eepromfw);

    devices.insert(std::move(eepromDevice));
}
