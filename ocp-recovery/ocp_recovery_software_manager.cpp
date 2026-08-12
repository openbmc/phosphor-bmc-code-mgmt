#include "ocp_recovery_software_manager.hpp"

#include "common/include/dbus_helper.hpp"
#include "common/include/software_manager.hpp"
#include "ocp_recovery_device.hpp"

#include <ocp/ocp_recovery.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/bus.hpp>

#include <algorithm>
#include <cstdint>

PHOSPHOR_LOG2_USING;

namespace OCPDevice = phosphor::software::ocp_recovery::device;
namespace SoftwareInf = phosphor::software;

const std::string configDBusName = "OCPRecovery";

static constexpr uint32_t defaultForceRecoveryTimeout = 30;

OCPRecoverySoftwareManager::OCPRecoverySoftwareManager(
    SDBusAsync::context& ctx) : ManagerInf::SoftwareManager(ctx, configDBusName)
{}

void OCPRecoverySoftwareManager::start()
{
    // initDevices is a coroutine holding a reference to this vector; it
    // must outlive ctx.run(), so a temporary argument would dangle.
    const std::vector<std::string> configIntfs = {
        "xyz.openbmc_project.Configuration.OCPRecoveryFirmware"};

    ctx.spawn(initDevices(configIntfs));
    ctx.run();
}

SDBusAsync::task<bool> OCPRecoverySoftwareManager::initDevice(
    const std::string& service, const sdbusplus::object_path& path,
    SoftwareConfig& config)
{
    std::string configIface =
        "xyz.openbmc_project.Configuration." + config.configType;

    std::optional<uint64_t> address =
        co_await dbusGetRequiredProperty<uint64_t>(ctx, service, path,
                                                   configIface, "Address");
    std::optional<uint64_t> busNum = co_await dbusGetRequiredProperty<uint64_t>(
        ctx, service, path, configIface, "Bus");

    std::optional<uint64_t> vendorIANA =
        co_await dbusGetRequiredProperty<uint64_t>(
            ctx, service, path, configIface + ".FirmwareInfo", "VendorIANA");
    std::optional<std::string> compatible =
        co_await dbusGetRequiredProperty<std::string>(
            ctx, service, path, configIface + ".FirmwareInfo",
            "CompatibleHardware");

    if (!address.has_value() || !busNum.has_value() ||
        !vendorIANA.has_value() || !compatible.has_value())
    {
        error("missing config property");
        co_return false;
    }

    std::optional<uint64_t> chunkSize =
        co_await dbusGetOptionalProperty<uint64_t>(ctx, service, path,
                                                   configIface, "ChunkSize");
    std::optional<uint64_t> memoryWindow =
        co_await dbusGetOptionalProperty<uint64_t>(ctx, service, path,
                                                   configIface, "MemoryWindow");
    std::optional<uint64_t> forceRecoveryTimeout =
        co_await dbusGetOptionalProperty<uint64_t>(
            ctx, service, path, configIface, "ForceRecoveryTimeoutSeconds");

    const size_t chunk =
        std::clamp<uint64_t>(chunkSize.value_or(ocp::recovery::chunkSizeMax), 1,
                             ocp::recovery::chunkSizeMax);
    const auto bus = static_cast<uint16_t>(busNum.value());

    const DeviceSettings settings{
        static_cast<uint16_t>(address.value()),
        chunk,
        static_cast<uint8_t>(memoryWindow.value_or(ocp::recovery::cmsDefault)),
        static_cast<uint32_t>(
            forceRecoveryTimeout.value_or(defaultForceRecoveryTimeout)),
        static_cast<uint32_t>(vendorIANA.value()),
        compatible.value()};

    // Same type devices share one device and one Software.Update object
    if (auto it = aggregateDevices.find(settings); it != aggregateDevices.end())
    {
        lg2::info(
            "[config] adding bus {BUS} to the aggregated OCP recovery device at Address: {ADDR}",
            "BUS", bus, "ADDR", lg2::hex, address.value());
        it->second->addBus(bus);
        co_return true;
    }

    lg2::info("[config] OCP recovery device on bus {BUS} at Address: {ADDR}",
              "BUS", bus, "ADDR", lg2::hex, address.value());

    auto ocpDevice = std::make_unique<OCPDevice::OCPRecoveryDevice>(
        ctx, std::vector<uint16_t>{bus}, std::get<0>(settings),
        std::get<1>(settings), std::get<2>(settings), std::get<3>(settings),
        config, this);

    std::unique_ptr<SoftwareInf::Software> software =
        std::make_unique<SoftwareInf::Software>(ctx, *ocpDevice);

    // The recovery protocol cannot read a version from the device
    software->setVersion(OCPDevice::recoveryVersion);

    software->enableUpdate({RequestedApplyTimes::Immediate});

    ocpDevice->softwareCurrent = std::move(software);

    aggregateDevices.emplace(settings, ocpDevice.get());

    devices.insert({config.objectPath, std::move(ocpDevice)});

    co_return true;
}

int main()
{
    sdbusplus::async::context ctx;

    OCPRecoverySoftwareManager ocpRecoverySoftwareManager(ctx);

    ocpRecoverySoftwareManager.start();
    return 0;
}
