#include "i2c_vr_device_code_updater.hpp"

#include "common/include/software_manager.hpp"
#include "i2c_vr_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

#include <cstdint>

const std::string configTypesI2CVRDevice = "XDPE152C4";

I2CVRDeviceCodeUpdater::I2CVRDeviceCodeUpdater(sdbusplus::async::context& ctx,
                                               bool isDryRun) :
    SoftwareManager(ctx, configTypesI2CVRDevice, isDryRun)
{}

std::set<std::string> I2CVRDeviceCodeUpdater::getEMConfigTypes()
{
    return {configTypesI2CVRDevice};
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<>
    I2CVRDeviceCodeUpdater::getInitialConfigurationSingleDevice(
        const std::string& service, DeviceConfig& config)
// NOLINTEND(readability-static-accessed-through-instance)
{
    const std::string& path = config.objPathConfig;

    std::optional<uint16_t> optBusNo =
        co_await SoftwareManager::dbusGetRequiredConfigurationProperty<
            uint16_t>(service, path, "Bus", config);
    std::optional<uint8_t> optAddr =
        co_await SoftwareManager::dbusGetRequiredConfigurationProperty<uint8_t>(
            service, path, "Address", config);
    std::optional<std::string> optVRChipType =
        co_await SoftwareManager::dbusGetRequiredConfigurationProperty<
            std::string>(service, path, "Type", config);

    if (!optBusNo.has_value() || !optAddr.has_value() ||
        !optVRChipType.has_value())
    {
        co_return;
    }

    lg2::debug(
        "[config] voltage regulator device type: {TYPE} on Bus: {BUS} at Address: {ADDR}",
        "TYPE", optVRChipType.value(), "BUS", optBusNo.value(), "ADDR",
        optAddr.value());

    auto i2cDevice = std::make_unique<I2CVRDevice>(
        ctx, dryRun, optVRChipType.value(), optBusNo.value(), optAddr.value(),
        config, this);

    std::string version = "unknown";

    std::unique_ptr<Software> vrfw =
        std::make_unique<Software>(ctx, *i2cDevice);

    vrfw->setVersion(version);

    std::set<RequestedApplyTimes> allowedApplyTime = {
        RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};

    vrfw->enableUpdate(allowedApplyTime);

    i2cDevice->softwareCurrent = std::move(vrfw);

    devices.insert(std::move(i2cDevice));
}
