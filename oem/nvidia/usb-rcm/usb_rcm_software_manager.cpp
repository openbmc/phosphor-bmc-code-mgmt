#include "usb_rcm_software_manager.hpp"

#include "common/include/dbus_helper.hpp"
#include "common/include/software_manager.hpp"
#include "usb_rcm_device.hpp"
#include "usb_rcm_driver.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/bus.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace USBRCMDevice = phosphor::software::usb_rcm::device;
namespace SoftwareInf = phosphor::software;
namespace UsbRcm = phosphor::software::usb_rcm;

const std::string configDBusName = "USBRCMRecovery";
const std::vector<std::string> emConfigTypes = {"USBRCMFirmware"};

// Fetch an optional EM property without the error log that
// dbusGetRequiredProperty emits for an absent property.
template <typename T>
static sdbusplus::async::task<std::optional<T>> dbusGetOptionalProperty(
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
    catch (const std::exception&)
    {
        debug("Optional property {PROPERTY} not set on {PATH}", "PROPERTY",
              property, "PATH", path);
    }
    co_return opt;
}

USBRCMRecoverySoftwareManager::USBRCMRecoverySoftwareManager(
    SDBusAsync::context& ctx) :
    ManagerInf::SoftwareManager(ctx, configDBusName)
{}

void USBRCMRecoverySoftwareManager::start()
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

SDBusAsync::task<bool> USBRCMRecoverySoftwareManager::initDevice(
    const std::string& service, const sdbusplus::object_path& path,
    SoftwareConfig& config)
{
    const std::string iface =
        "xyz.openbmc_project.Configuration." + config.configType;

    std::optional<std::string> usbPort =
        co_await dbusGetRequiredProperty<std::string>(ctx, service, path, iface,
                                                      "USBPort");
    if (!usbPort)
    {
        error("USBRCMRecovery config '{NAME}': missing required USBPort", "NAME",
              config.configName);
        co_return false;
    }

    UsbRcm::DeviceConfig dc;
    dc.name = config.configName;
    dc.usbPort = *usbPort;

    std::optional<std::string> configTypeStr =
        co_await dbusGetOptionalProperty<std::string>(ctx, service, path, iface,
                                                      "ConfigType");
    if (configTypeStr)
    {
        if (auto configType = UsbRcm::parseConfigType(*configTypeStr))
        {
            dc.configType = *configType;
        }
        else
        {
            error("USBRCMRecovery config '{NAME}': invalid ConfigType '{TYPE}'",
                  "NAME", config.configName, "TYPE", *configTypeStr);
            co_return false;
        }
    }

    std::optional<std::string> dotBlobDir =
        co_await dbusGetOptionalProperty<std::string>(ctx, service, path, iface,
                                                      "DotBlobDir");
    if (dotBlobDir && !dotBlobDir->empty())
    {
        dc.dotBlobDir = *dotBlobDir;
    }

    std::optional<uint64_t> vendorId =
        co_await dbusGetOptionalProperty<uint64_t>(ctx, service, path, iface,
                                                   "VendorId");
    if (vendorId)
    {
        dc.vendorId = static_cast<uint16_t>(*vendorId);
    }
    std::optional<uint64_t> productId =
        co_await dbusGetOptionalProperty<uint64_t>(ctx, service, path, iface,
                                                   "ProductId");
    if (productId)
    {
        dc.productId = static_cast<uint16_t>(*productId);
    }

    // Recovery interface/endpoint overrides. RecoveryEndpoint defaults to 0
    // (auto-discover the interface's bulk-OUT); set it only to pin a specific
    // endpoint number on a revision the discovery cannot handle.
    std::optional<uint64_t> recoveryInterface =
        co_await dbusGetOptionalProperty<uint64_t>(ctx, service, path, iface,
                                                   "RecoveryInterface");
    if (recoveryInterface)
    {
        dc.recoveryInterface = static_cast<int>(*recoveryInterface);
    }
    std::optional<uint64_t> recoveryEndpoint =
        co_await dbusGetOptionalProperty<uint64_t>(ctx, service, path, iface,
                                                   "RecoveryEndpoint");
    if (recoveryEndpoint)
    {
        dc.recoveryEndpoint = static_cast<uint8_t>(*recoveryEndpoint);
    }

    info("USBRCMRecovery: configured device '{NAME}' on USB port {PORT}", "NAME",
         dc.name, "PORT", dc.usbPort);

    auto device = std::make_unique<USBRCMDevice::USBRCMRecoveryDevice>(
        ctx, std::move(dc), config, this);

    std::unique_ptr<SoftwareInf::Software> software =
        std::make_unique<SoftwareInf::Software>(ctx, *device);

    // A device parked in recovery cannot report a firmware version; the
    // framework fills in the real version from the update package after a
    // successful recovery.
    software->setVersion(
        "unknown", SoftwareInf::SoftwareVersion::VersionPurpose::Other);
    software->enableUpdate({RequestedApplyTimes::Immediate});

    device->softwareCurrent = std::move(software);
    devices.insert({config.objectPath, std::move(device)});

    co_return true;
}

int main()
{
    sdbusplus::async::context ctx;

    USBRCMRecoverySoftwareManager usbRcmRecoverySoftwareManager(ctx);

    usbRcmRecoverySoftwareManager.start();
    return 0;
}
