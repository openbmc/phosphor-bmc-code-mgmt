#include "software_manager.hpp"

#include <boost/container/flat_map.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>
#include <xyz/openbmc_project/Software/Version/client.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

#include <cstdint>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software::manager;

using AsyncMatch = sdbusplus::async::match;

namespace RulesIntf = sdbusplus::match_rules;
static constexpr auto serviceNameEM = "xyz.openbmc_project.EntityManager";

const auto matchRuleSender = RulesIntf::sender(serviceNameEM);
const auto matchRulePath = RulesIntf::path("/xyz/openbmc_project/inventory");

static constexpr std::string_view fwInfoSuffix = ".FirmwareInfo";
static constexpr std::string_view emConfPrefix =
    "xyz.openbmc_project.Configuration.";

using ManagedObjectType = std::map<sdbusplus::object_path, InterfacesMap>;

SoftwareManager::SoftwareManager(sdbusplus::async::context& ctx,
                                 const std::string& serviceNameSuffix) :
    ctx(ctx),
    configIntfAddedMatch(ctx, RulesIntf::interfacesAdded() + matchRuleSender),
    configIntfRemovedMatch(ctx, RulesIntf::interfacesRemoved() + matchRulePath),
    serviceName("xyz.openbmc_project.Software." + serviceNameSuffix),
    manager(ctx, sdbusplus::client::xyz::openbmc_project::software::Version<>::
                     namespace_path)
{
    debug("requesting dbus name {BUSNAME}", "BUSNAME", serviceName);

    ctx.request_name(serviceName.c_str());

    debug("Initialized SoftwareManager");
}

template <typename T>
static std::optional<T> getProp(const DbusPropertyMap& m,
                                const std::string& key)
{
    if (auto it = m.find(key); it != m.end())
    {
        if (const auto* v = std::get_if<T>(&it->second))
        {
            return *v;
        }
    }
    return std::nullopt;
}

static std::optional<SoftwareConfig> getConfig(
    const sdbusplus::object_path& objectPath, InterfacesMap& interfacesMap,
    const std::string& configInterface, const std::string& fwIface)
{
    auto baseIt = interfacesMap.find(configInterface);
    if (baseIt == interfacesMap.end())
    {
        error("Base interface {IFACE} missing for {PATH}", "IFACE",
              configInterface, "PATH", objectPath);
        return std::nullopt;
    }
    const auto& baseProps = baseIt->second;

    auto fwIt = interfacesMap.find(fwIface);
    if (fwIt == interfacesMap.end())
    {
        error("Firmware info interface {IFACE} missing for {PATH}", "IFACE",
              fwIface, "PATH", objectPath);
        return std::nullopt;
    }
    const auto& fwInfoProps = fwIt->second;

    auto vendorIANA = getProp<uint64_t>(fwInfoProps, "VendorIANA");
    auto compatible = getProp<std::string>(fwInfoProps, "CompatibleHardware");
    if (!vendorIANA || !compatible)
    {
        error("Missing or invalid VendorIANA/CompatibleHardware at {PATH}",
              "PATH", objectPath);
        return std::nullopt;
    }

    auto configType = getProp<std::string>(baseProps, "FirmwareType");
    if (!configType)
    {
        configType = getProp<std::string>(baseProps, "Type");
    }

    auto configName = getProp<std::string>(baseProps, "FirmwareName");
    if (!configName)
    {
        configName = getProp<std::string>(baseProps, "Name");
    }

    if (!configType || !configName)
    {
        error("Missing or invalid FirmwareType/Name at {PATH}", "PATH",
              objectPath);
        return std::nullopt;
    }

    return SoftwareConfig(objectPath, static_cast<uint32_t>(*vendorIANA),
                          *compatible, *configType, *configName,
                          configInterface, std::move(interfacesMap));
}

std::optional<SoftwareConfig> SoftwareManager::processIncomingInterface(
    const std::string& objPathStr, const std::string& interfaceName,
    const DbusPropertyMap& props)
{
    std::string baseIface;

    if (interfaceName.ends_with(fwInfoSuffix))
    {
        baseIface =
            interfaceName.substr(0, interfaceName.size() - fwInfoSuffix.size());
    }
    else if (interfaceName.starts_with(emConfPrefix) &&
             interfaceName.find('.', emConfPrefix.size()) == std::string::npos)
    {
        auto configType = getProp<std::string>(props, "FirmwareType");
        if (!configType)
        {
            configType = getProp<std::string>(props, "Type");
        }
        if (!configType || !this->isSupported(*configType))
        {
            return std::nullopt;
        }
        baseIface = interfaceName;
    }
    else
    {
        return std::nullopt;
    }

    auto& cached = pendingSignals[objPathStr];
    cached[interfaceName] = props;

    std::string fwIface = baseIface + std::string(fwInfoSuffix);
    if (cached.contains(baseIface) && cached.contains(fwIface))
    {
        sdbusplus::object_path objPath(objPathStr);
        auto optConfig = getConfig(objPath, cached, baseIface, fwIface);
        pendingSignals.erase(objPathStr);
        return optConfig;
    }

    return std::nullopt;
}

sdbusplus::async::task<> SoftwareManager::initDevices()
{
    ctx.spawn(interfaceAddedMatch());
    ctx.spawn(interfaceRemovedMatch());

    auto client = sdbusplus::async::proxy()
                      .service(serviceNameEM)
                      .path("/xyz/openbmc_project/inventory")
                      .interface("org.freedesktop.DBus.ObjectManager");

    ManagedObjectType managedObjects;
    try
    {
        managedObjects =
            co_await client.call<ManagedObjectType>(ctx, "GetManagedObjects");
    }
    catch (std::exception& e)
    {
        error("GetManagedObjects failed: {ERROR}", "ERROR", e);
        co_return;
    }

    for (const auto& [objPath, interfacesMap] : managedObjects)
    {
        for (const auto& [interfaceName, props] : interfacesMap)
        {
            if (auto optConfig =
                    processIncomingInterface(objPath.str, interfaceName, props))
            {
                co_await handleInterfaceAdded(serviceNameEM, objPath,
                                              std::move(*optConfig));
                break;
            }
        }
    }

    debug("Done with initial configuration");
}

std::string SoftwareManager::getBusName()
{
    return serviceName;
}

sdbusplus::async::task<void> SoftwareManager::handleInterfaceAdded(
    const std::string& service, const sdbusplus::object_path& path,
    SoftwareConfig config)
{
    if (devices.contains(path) || initializingPaths.contains(path))
    {
        debug("Skipping duplicate init for {PATH}", "PATH", path);
        co_return;
    }

    initializingPaths.insert(path);
    debug("Found configuration interface at {SERVICE}, {PATH}", "SERVICE",
          service, "PATH", path);

    const bool accepted = co_await initDevice(service, path, config);

    if (accepted && devices.contains(config.objectPath))
    {
        auto& device = devices[config.objectPath];

        if (device->softwareCurrent)
        {
            co_await device->softwareCurrent->createInventoryAssociations(true);

            device->softwareCurrent->setActivation(
                SoftwareActivation::Activations::Active);
        }
    }
    initializingPaths.erase(path);

    co_return;
}

sdbusplus::async::task<void> SoftwareManager::interfaceAddedMatch()
{
    while (!ctx.stop_requested())
    {
        auto nextResult = co_await configIntfAddedMatch
                              .next<sdbusplus::object_path, InterfacesMap>();

        auto& [objPath, interfacesMap] = nextResult;

        for (const auto& [interfaceName, props] : interfacesMap)
        {
            if (auto optConfig =
                    processIncomingInterface(objPath.str, interfaceName, props))
            {
                debug("detected interface {INTF} added on {PATH}", "INTF",
                      optConfig->baseInterface, "PATH", objPath);
                co_await handleInterfaceAdded(serviceNameEM, objPath,
                                              std::move(*optConfig));
                break;
            }
        }
    }
}

sdbusplus::async::task<void> SoftwareManager::interfaceRemovedMatch()
{
    while (!ctx.stop_requested())
    {
        auto nextResult =
            co_await configIntfRemovedMatch
                .next<sdbusplus::object_path, std::vector<std::string>>();

        auto& [objPath, interfacesRemoved] = nextResult;

        debug("detected interface removed on {PATH}", "PATH", objPath);

        for (const auto& interfaceName : interfacesRemoved)
        {
            if (interfaceName.starts_with(emConfPrefix))
            {
                pendingSignals.erase(objPath.str);
                if (devices.contains(objPath))
                {
                    debug("detected interface {INTF} removed on {PATH}", "INTF",
                          interfaceName, "PATH", objPath);
                    co_await handleInterfaceRemoved(objPath);
                }
                break;
            }
        }
    }
}

sdbusplus::async::task<void> SoftwareManager::handleInterfaceRemoved(
    const sdbusplus::object_path& objPath)
{
    if (!devices.contains(objPath))
    {
        debug("could not find a device to remove");
        co_return;
    }

    if (devices[objPath]->updateInProgress)
    {
        // TODO: This code path needs to be cleaned up in the future to
        // eventually remove the device.
        debug(
            "removal of device at {PATH} ignored because of in-progress update",
            "PATH", objPath.str);
        co_return;
    }

    debug("removing device at {PATH}", "PATH", objPath.str);
    devices.erase(objPath);
}
