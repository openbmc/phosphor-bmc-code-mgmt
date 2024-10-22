#include "i2c_vr_device_code_updater.hpp"
#include "common/include/software_manager.hpp"
#include "i2c_vr_device.hpp"
#include "vr.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

#include <cstdint>


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
    std::optional<T> opt = std::nullopt;
    try
    {
        std::variant<T> result =
            co_await client.call<std::variant<T>>(ctx, "Get", intf, property);
        opt = std::get<T>(result);
    }
    catch (std::exception& e)
    {
        error(
            "[config] missing property {PROPERTY} on path {PATH}, interface {INTF}",
            "PROPERTY", property, "PATH", path, "INTF", intf);
    }
    co_return opt;
}

I2CVRSoftwareManager::I2CVRSoftwareManager(sdbusplus::async::context& ctx) :
    SoftwareManager(ctx, configDBusName)
{}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool>
    I2CVRSoftwareManager::initDevice(
        const std::string& service, const std::string& path, SoftwareConfig& config)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::string configIface ="xyz.openbmc_project.Configuration." + config.configType;

    std::optional<uint16_t> busNo =
        co_await dbusGetRequiredProperty<uint16_t>(ctx, service, path, configIface, "Bus");
    std::optional<uint8_t> address =
        co_await dbusGetRequiredProperty<uint8_t>(ctx, service, path, configIface, "Address");
    std::optional<std::string> vrChipType =
        co_await dbusGetRequiredProperty<std::string>(ctx, service, path, configIface, "Type");

    if (!busNo.has_value() || !address.has_value() ||
        !vrChipType.has_value())
    {
        error("missing config property");
        co_return false;
    }

    enum VRType vrType = stringToEnum(vrChipType.value());
    // Check correctness of vrtype

    lg2::debug(
        "[config] Voltage regulator device type: {TYPE} on Bus: {BUS} at Address: {ADDR}",
        "TYPE", vrChipType.value(), "BUS", busNo.value(), "ADDR",
        address.value());

    auto i2cDevice = std::make_unique<phosphor::software::i2c_vr::device::I2CVRDevice>(
        ctx, vrType, busNo.value(), address.value(),
        config, this);

    std::string version = "unknown";

    std::unique_ptr<Software> software =
        std::make_unique<Software>(ctx, *i2cDevice);

    uint32_t sum = 0;
    if (i2cDevice->vrInterface->getCRC(&sum) < 0) {
        co_return false;
    }

    software->setVersion(std::to_string(sum));

    std::set<RequestedApplyTimes> allowedApplyTime = {
        RequestedApplyTimes::Immediate};

    software->enableUpdate(allowedApplyTime);

    i2cDevice->softwareCurrent = std::move(software);

    devices.insert({config.objectPath, std::move(i2cDevice)});
}
