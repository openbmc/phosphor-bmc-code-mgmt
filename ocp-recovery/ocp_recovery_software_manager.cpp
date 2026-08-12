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

    if (!address.has_value() || !busNum.has_value())
    {
        error("missing config property");
        co_return false;
    }

    // All OCPRecoveryFirmware configs aggregate into one device (and one
    // Software.Update object): later configs only contribute their bus.
    if (aggregateDevice != nullptr)
    {
        lg2::info(
            "[config] adding bus {BUS} to the aggregated OCP recovery device",
            "BUS", busNum.value());
        aggregateDevice->addBus(static_cast<uint16_t>(busNum.value()));
        co_return true;
    }

    std::vector<uint16_t> buses = {static_cast<uint16_t>(busNum.value())};

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

    lg2::info(
        "[config] OCP recovery device(s) on {COUNT} bus(es) at Address: {ADDR}",
        "COUNT", buses.size(), "ADDR", lg2::hex, address.value());

    auto ocpDevice = std::make_unique<OCPDevice::OCPRecoveryDevice>(
        ctx, std::move(buses), static_cast<uint16_t>(address.value()), chunk,
        static_cast<uint8_t>(memoryWindow.value_or(ocp::recovery::cmsDefault)),
        static_cast<uint32_t>(
            forceRecoveryTimeout.value_or(defaultForceRecoveryTimeout)),
        config, this);

    std::unique_ptr<SoftwareInf::Software> software =
        std::make_unique<SoftwareInf::Software>(ctx, *ocpDevice);

    // A device that requires recovery cannot report a firmware version;
    // the framework fills in the real version from the update package
    // after a successful recovery.
    software->setVersion("unknown",
                         SoftwareInf::SoftwareVersion::VersionPurpose::Other);

    software->enableUpdate({RequestedApplyTimes::Immediate});

    ocpDevice->softwareCurrent = std::move(software);

    aggregateDevice = ocpDevice.get();

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
