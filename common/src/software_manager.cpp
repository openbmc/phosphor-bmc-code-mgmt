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

SoftwareManager::SoftwareManager(sdbusplus::async::context& ctx,
                                 const std::string& serviceNameSuffix) :
    ctx(ctx), serviceNameSuffix(serviceNameSuffix),
    manager(ctx, sdbusplus::client::xyz::openbmc_project::software::Version<>::
                     namespace_path)
{
    const std::string serviceNameFull =
        "xyz.openbmc_project.Software." + serviceNameSuffix;

    debug("requesting dbus name {BUSNAME}", "BUSNAME", serviceNameFull);

    ctx.request_name(serviceNameFull.c_str());

    debug("Initialized SoftwareManager");
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<> SoftwareManager::initDevices(
    const std::vector<std::string>& configurationInterfaces)
// NOLINTEND(readability-static-accessed-through-instance)
{
    co_await initConfigIntfMatches(configurationInterfaces);

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

            co_await handleConfigurationInterfaceFound(service, path,
                                                       interfaceFound);
        }
    }

    debug("[config] done with initial configuration");
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<std::optional<SoftwareConfig>>
    SoftwareManager::fetchSoftwareConfig(
        sdbusplus::async::context& ctx, const std::string& service,
        const std::string& objectPath, const std::string& interfaceFound)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto client = sdbusplus::async::proxy()
                      .service(service)
                      .path(objectPath)
                      .interface("org.freedesktop.DBus.Properties");

    uint64_t vendorIANA = 0;
    std::string compatible{};
    std::string emConfigType{};
    std::string emConfigName{};

    const std::string ifaceFwInfoDef = interfaceFound + ".FirmwareInfo";

    try
    {
        {
            auto propVendorIANA = co_await client.call<std::variant<uint64_t>>(
                ctx, "Get", ifaceFwInfoDef, "VendorIANA");

            vendorIANA = std::get<uint64_t>(propVendorIANA);
        }
        {
            auto propCompatible =
                co_await client.call<std::variant<std::string>>(
                    ctx, "Get", ifaceFwInfoDef, "CompatibleHardware");

            compatible = std::get<std::string>(propCompatible);
        }
        {
            auto propEMConfigType =
                co_await client.call<std::variant<std::string>>(
                    ctx, "Get", interfaceFound, "Type");

            emConfigType = std::get<std::string>(propEMConfigType);
        }
        {
            auto propEMConfigName =
                co_await client.call<std::variant<std::string>>(
                    ctx, "Get", interfaceFound, "Name");

            emConfigName = std::get<std::string>(propEMConfigName);
        }
    }
    catch (std::exception& e)
    {
        error(e.what());
        co_return std::nullopt;
    }

    SoftwareConfig config(objectPath, vendorIANA, compatible, emConfigType,
                          emConfigName);

    co_return config;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<void> SoftwareManager::handleConfigurationInterfaceFound(
    const std::string& service, const std::string& path,
    const std::string& interface)
// NOLINTEND(readability-static-accessed-through-instance)
{
    debug("[config] found configuration interface at {SERVICE}, {OBJPATH}",
          "SERVICE", service, "OBJPATH", path);

    auto optConfig = co_await SoftwareManager::fetchSoftwareConfig(
        ctx, service, path, interface);

    if (!optConfig.has_value())
    {
        error("Error fetching common configuration from {PATH}", "PATH", path);
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

static constexpr auto serviceNameEM = "xyz.openbmc_project.EntityManager";

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<void> SoftwareManager::initConfigIntfMatches(
    const std::vector<std::string>& configInterfaces)
// NOLINTEND(readability-static-accessed-through-instance)
{
    if (configIntfAddedMatch != nullptr && configIntfRemovedMatch != nullptr)
    {
        error(
            "map of configuration interface added/removed matches was already initialized");
        co_return;
    }

    namespace RulesIntf = sdbusplus::bus::match::rules;

    using AsyncMatch = sdbusplus::async::match;

    debug("Adding interface added/removed matches");

    auto matchRuleSender = RulesIntf::sender(serviceNameEM);
    auto matchRulePath = RulesIntf::path("/xyz/openbmc_project/inventory");

    auto matchStrAdded = RulesIntf::interfacesAdded() + matchRuleSender;
    auto matchStrRemoved = RulesIntf::interfacesRemoved() + matchRulePath;

    info("Register match: {STR}", "STR", matchStrAdded);
    info("Register match: {STR}", "STR", matchStrRemoved);

    configIntfAddedMatch = std::make_unique<AsyncMatch>(ctx, matchStrAdded);
    configIntfRemovedMatch = std::make_unique<AsyncMatch>(ctx, matchStrRemoved);

    ctx.spawn(awaitConfigIntfAddedMatch(configInterfaces));
    ctx.spawn(awaitConfigIntfRemovedMatch(configInterfaces));

    co_return;
}

using BasicVariantType =
    std::variant<std::vector<std::string>, std::string, int64_t, uint64_t,
                 double, int32_t, uint32_t, int16_t, uint16_t, uint8_t, bool>;
using InterfacesMap = boost::container::flat_map<std::string, BasicVariantType>;
using ConfigMap = boost::container::flat_map<std::string, InterfacesMap>;

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<void> SoftwareManager::awaitConfigIntfAddedMatch(
    std::vector<std::string> interfaces)
// NOLINTEND(readability-static-accessed-through-instance)
{
    while (!ctx.stop_requested())
    {
        std::tuple<std::string, ConfigMap> nextResult("", {});
        nextResult = co_await configIntfAddedMatch
                         ->next<sdbusplus::message::object_path, ConfigMap>();

        auto& [objPath, interfacesMap] = nextResult;

        for (auto& [intf, propValueMap] : interfacesMap)
        {
            debug("detected interface {INTF} added on {PATH}", "INTF", intf,
                  "PATH", objPath);

            for (auto& configIntf : interfaces)
            {
                if (configIntf == intf)
                {
                    co_await handleConfigurationInterfaceFound(
                        serviceNameEM, objPath, configIntf);
                }
            }
        }
    }
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<void> SoftwareManager::awaitConfigIntfRemovedMatch(
    std::vector<std::string> interfaces)
// NOLINTEND(readability-static-accessed-through-instance)
{
    while (!ctx.stop_requested())
    {
        auto nextResult = co_await configIntfRemovedMatch->next<
            sdbusplus::message::object_path, std::vector<std::string>>();

        auto [objPath, interfacesRemoved] = nextResult;

        debug("detected interface removed on {PATH}", "PATH", objPath);

        bool configIntfRemoved = false;
        for (auto& intf : interfaces)
        {
            if (std::find(interfacesRemoved.begin(), interfacesRemoved.end(),
                          intf) != interfacesRemoved.end())
            {
                debug("detected interface {INTF} removed on {PATH}", "INTF",
                      intf, "PATH", objPath);
                configIntfRemoved = true;
            }
        }

        if (!configIntfRemoved)
        {
            continue;
        }

        sdbusplus::message::object_path deviceToRemove;

        for (auto& [path, device] : devices)
        {
            if (device->config.objectPath != objPath)
            {
                continue;
            }

            // the device's config was found on this object path

            if (device->updateInProgress)
            {
                // TODO: This code path needs to be cleaned up in the future to
                // eventually remove the device.
                debug(
                    "removal of device at {PATH} ignored because of in-progress update",
                    "PATH", path.str);
                continue;
            }

            deviceToRemove = path;
        }

        if (deviceToRemove.str.empty())
        {
            debug("could not find a device to remove");
        }
        else
        {
            debug("removing device at {PATH}", "PATH", deviceToRemove.str);
            devices.erase(deviceToRemove);
        }
    }
}
