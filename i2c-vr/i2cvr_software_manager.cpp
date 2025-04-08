#include "i2cvr_software_manager.hpp"

#include "common/include/dbus_helper.hpp"
#include "common/include/software_manager.hpp"
#include "i2cvr_device.hpp"
#include "vr.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

#include <cstdint>

PHOSPHOR_LOG2_USING;

namespace VR = phosphor::software::VR;
namespace I2CDevice = phosphor::software::i2c_vr::device;
namespace SoftwareInf = phosphor::software;
namespace ManagerInf = phosphor::software::manager;

const std::string configDBusName = "I2CVR";
const std::vector<std::string> emConfigTypes = {"XDPE1X2XXFirmware",
                                                "ISL69269Firmware",
                                                "DummyDeviceFirmware"};

I2CVRSoftwareManager::I2CVRSoftwareManager(sdbusplus::async::context& ctx) :
    ManagerInf::SoftwareManager(ctx, configDBusName)
{}

void I2CVRSoftwareManager::start()
{
    std::vector<std::string> configIntfs;
    configIntfs.reserve(emConfigTypes.size());
    for (auto& name : emConfigTypes)
    {
        configIntfs.push_back("xyz.openbmc_project.Configuration." + name);
    }

    ctx.spawn(initDevices(configIntfs));
    ctx.run();
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> I2CVRSoftwareManager::initDevice(
    const std::string& service, const std::string& path, SoftwareConfig& config)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::string configIface =
        "xyz.openbmc_project.Configuration." + config.configType;

    std::optional<uint64_t> busNum = co_await dbusGetRequiredProperty<uint64_t>(
        ctx, service, path, configIface, "Bus");
    std::optional<uint64_t> address =
        co_await dbusGetRequiredProperty<uint64_t>(ctx, service, path,
                                                   configIface, "Address");
    std::optional<std::string> vrChipType =
        co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                      configIface, "Type");

    if (!busNum.has_value() || !address.has_value() || !vrChipType.has_value())
    {
        error("missing config property");
        co_return false;
    }

    VR::VRType vrType;
    if (!VR::stringToEnum(vrChipType.value(), vrType))
    {
        error("unknown voltage regulator type: {TYPE}", "TYPE",
              vrChipType.value());
        co_return false;
    }

    lg2::debug(
        "[config] Voltage regulator device type: {TYPE} on Bus: {BUS} at Address: {ADDR}",
        "TYPE", vrChipType.value(), "BUS", busNum.value(), "ADDR",
        address.value());

    auto i2cDevice = std::make_unique<I2CDevice::I2CVRDevice>(
        ctx, vrType, static_cast<uint16_t>(busNum.value()),
        static_cast<uint16_t>(address.value()), config, this);

    std::unique_ptr<SoftwareInf::Software> software =
        std::make_unique<SoftwareInf::Software>(ctx, *i2cDevice);

    uint32_t sum;
    if (!(co_await i2cDevice->getVersion(&sum)))
    {
        error("unable to obtain Version/CRC from voltage regulator");
        co_return false;
    }

    software->setVersion(std::to_string(sum));

    std::set<RequestedApplyTimes> allowedApplyTime = {
        RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};

    software->enableUpdate(allowedApplyTime);

    i2cDevice->softwareCurrent = std::move(software);

    devices.insert({config.objectPath, std::move(i2cDevice)});

    co_return true;
}

int main()
{
    sdbusplus::async::context ctx;

    I2CVRSoftwareManager i2cVRSoftwareManager(ctx);

    i2cVRSoftwareManager.start();
    return 0;
}
