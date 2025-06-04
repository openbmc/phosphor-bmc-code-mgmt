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

namespace RulesIntf = sdbusplus::bus::match::rules;
static constexpr auto serviceNameEM = "xyz.openbmc_project.EntityManager";

const auto matchRuleSender = RulesIntf::sender(serviceNameEM);
const auto matchRulePath = RulesIntf::path("/xyz/openbmc_project/inventory");

SoftwareManager::SoftwareManager(sdbusplus::async::context& ctx,
                                 const std::string& serviceNameSuffix) :
    ctx(ctx),
    configIntfAddedMatch(ctx, RulesIntf::interfacesAdded() + matchRuleSender),
    configIntfRemovedMatch(ctx, RulesIntf::interfacesRemoved() + matchRulePath),
    serviceNameSuffix(serviceNameSuffix),
    manager(ctx, sdbusplus::client::xyz::openbmc_project::software::Version<>::
                     namespace_path)
{
    const std::string serviceNameFull =
        "xyz.openbmc_project.Software." + serviceNameSuffix;

    debug("requesting dbus name {BUSNAME}", "BUSNAME", serviceNameFull);

    ctx.request_name(serviceNameFull.c_str());

    debug("Initialized SoftwareManager");
}

static sdbusplus::async::task<std::optional<SoftwareConfig>> getConfig(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& objectPath, const std::string& interfacePrefix)
{
    auto client = sdbusplus::async::proxy()
                      .service(service)
                      .path(objectPath)
                      .interface("org.freedesktop.DBus.Properties");

    uint64_t vendorIANA = 0;
    std::string compatible{};
    std::string configType{};
    std::string configName{};

    const std::string interfaceName = interfacePrefix + ".FirmwareInfo";

    try
    {
        {
            auto propVendorIANA = co_await client.call<std::variant<uint64_t>>(
                ctx, "Get", interfaceName, "VendorIANA");

            vendorIANA = std::get<uint64_t>(propVendorIANA);
        }
        {
            auto propCompatible =
                co_await client.call<std::variant<std::string>>(
                    ctx, "Get", interfaceName, "CompatibleHardware");

            compatible = std::get<std::string>(propCompatible);
        }
        {
            auto propConfigType =
                co_await client.call<std::variant<std::string>>(
                    ctx, "Get", interfacePrefix, "Type");

            configType = std::get<std::string>(propConfigType);
        }
        {
            auto propConfigName =
                co_await client.call<std::variant<std::string>>(
                    ctx, "Get", interfacePrefix, "Name");

            configName = std::get<std::string>(propConfigName);
        }
    }
    catch (std::exception& e)
    {
        error("Failed to get config with {ERROR}", "ERROR", e);
        co_return std::nullopt;
    }

    co_return SoftwareConfig(objectPath, vendorIANA, compatible, configType,
                             configName);
}

sdbusplus::async::task<> SoftwareManager::initDevices(
    const std::vector<std::string>& configurationInterfaces)
{
    ctx.spawn(interfaceAddedMatch(configurationInterfaces));
    ctx.spawn(interfaceRemovedMatch(configurationInterfaces));

    auto client = sdbusplus::client::xyz::openbmc_project::ObjectMapper<>(ctx)
                      .service("xyz.openbmc_project.ObjectMapper")
                      .path("/xyz/openbmc_project/object_mapper");

    auto res = co_await client.get_sub_tree("/xyz/openbmc_project/inventory", 0,
                                            configurationInterfaces);

    for (auto& iface : configurationInterfaces)
    {
        debug("[config] looking for dbus interface {INTF}", "INTF", iface);
    }

    for (auto& [path, v] : res)
    {
        for (auto& [service, interfaceNames] : v)
        {
            std::string interfaceFound;

            for (std::string& interfaceName : interfaceNames)
            {
                for (auto& iface : configurationInterfaces)
                {
                    if (interfaceName == iface)
                    {
                        interfaceFound = interfaceName;
                    }
                }
            }

            if (interfaceFound.empty())
            {
                continue;
            }

            co_await handleInterfaceAdded(service, path, interfaceFound);
        }
    }

    debug("Done with initial configuration");
}

sdbusplus::async::task<void> SoftwareManager::handleInterfaceAdded(
    const std::string& service, const std::string& path,
    const std::string& interface)
{
    debug("Found configuration interface at {SERVICE}, {PATH}", "SERVICE",
          service, "PATH", path);

    auto optConfig = co_await getConfig(ctx, service, path, interface);

    if (!optConfig.has_value())
    {
        error("Failed to get configuration from {PATH}", "PATH", path);
        co_return;
    }

    if (devices.contains(optConfig.value().objectPath))
    {
        error("Device configured from {PATH} is already known", "PATH",
              optConfig.value().objectPath);
        co_return;
    }

    co_await initDevice(service, path, optConfig.value());

    co_return;
}

using BasicVariantType =
    std::variant<std::vector<std::string>, std::string, int64_t, uint64_t,
                 double, int32_t, uint32_t, int16_t, uint16_t, uint8_t, bool>;
using InterfacesMap = boost::container::flat_map<std::string, BasicVariantType>;
using ConfigMap = boost::container::flat_map<std::string, InterfacesMap>;

sdbusplus::async::task<void> SoftwareManager::interfaceAddedMatch(
    std::vector<std::string> interfaces)
{
    while (!ctx.stop_requested())
    {
        std::tuple<std::string, ConfigMap> nextResult("", {});
        nextResult = co_await configIntfAddedMatch
                         .next<sdbusplus::message::object_path, ConfigMap>();

        auto& [objPath, interfacesMap] = nextResult;

        for (auto& interface : interfaces)
        {
            if (interfacesMap.contains(interface))
            {
                debug("detected interface {INTF} added on {PATH}", "INTF",
                      interface, "PATH", objPath);

                co_await handleInterfaceAdded(serviceNameEM, objPath,
                                              interface);
            }
        }
    }
}

sdbusplus::async::task<void> SoftwareManager::interfaceRemovedMatch(
    std::vector<std::string> interfaces)
{
    while (!ctx.stop_requested())
    {
        auto nextResult = co_await configIntfRemovedMatch.next<
            sdbusplus::message::object_path, std::vector<std::string>>();

        auto& [objPath, interfacesRemoved] = nextResult;

        debug("detected interface removed on {PATH}", "PATH", objPath);

        for (auto& interface : interfaces)
        {
            if (std::ranges::find(interfacesRemoved, interface) !=
                interfacesRemoved.end())
            {
                debug("detected interface {INTF} removed on {PATH}", "INTF",
                      interface, "PATH", objPath);
                co_await handleInterfaceRemoved(objPath);
            }
        }
    }
}

sdbusplus::async::task<void> SoftwareManager::handleInterfaceRemoved(
    const sdbusplus::message::object_path& objPath)
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
