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
sdbusplus::async::task<void> SoftwareManager::handleConfigurationInterfaceFound(
    const std::string& service, const std::string& path,
    const std::string& interface)
// NOLINTEND(readability-static-accessed-through-instance)
{
    debug("[config] found configuration interface at {SERVICE}, {OBJPATH}",
          "SERVICE", service, "OBJPATH", path);

    auto optConfig =
        co_await SoftwareConfig::fetchFromDbus(ctx, service, path, interface);

    if (!optConfig.has_value())
    {
        error("Error fetching common configuration from {PATH}", "PATH", path);
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
    if (!configIntfAddedRemovedMatches.empty())
    {
        error(
            "map of configuration interface added/removed matches was already initialized");
        co_return;
    }

    namespace RulesIntf = sdbusplus::bus::match::rules;

    using AsyncMatch = sdbusplus::async::match;

    for (auto& intf : configInterfaces)
    {
        debug("Adding interface added/removed matches for {INTF}", "INTF",
              intf);

        auto matchRuleSender = RulesIntf::sender(serviceNameEM);

        auto matchStrAdded = RulesIntf::interfacesAdded() + matchRuleSender;
        auto matchStrRemoved = RulesIntf::interfacesRemoved() + matchRuleSender;

        info("Register match: {STR}", "STR", matchStrAdded);
        info("Register match: {STR}", "STR", matchStrRemoved);

        auto m1 = std::make_unique<AsyncMatch>(ctx, matchStrAdded);
        auto m2 = std::make_unique<AsyncMatch>(ctx, matchStrRemoved);

        configIntfAddedRemovedMatches.emplace(
            intf, std::tuple<std::unique_ptr<AsyncMatch>,
                             std::unique_ptr<AsyncMatch>>(
                      std::tuple(std::move(m1), std::move(m2))));

        ctx.spawn(awaitConfigIntfAddedMatch(intf));
        ctx.spawn(awaitConfigIntfRemovedMatch(intf));
    }

    co_return;
}

using BasicVariantType =
    std::variant<std::vector<std::string>, std::string, int64_t, uint64_t,
                 double, int32_t, uint32_t, int16_t, uint16_t, uint8_t, bool>;
using BaseConfigMap = boost::container::flat_map<std::string, BasicVariantType>;
using ConfigMap = boost::container::flat_map<std::string, BaseConfigMap>;

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<void> SoftwareManager::awaitConfigIntfAddedMatch(
    std::string intf)
// NOLINTEND(readability-static-accessed-through-instance)
{
    if (!configIntfAddedRemovedMatches.contains(intf))
    {
        error("internal map of dbus matches corrupted, missing {INTF}", "INTF",
              intf);
        co_return;
    }

    while (!ctx.stop_requested())
    {
        std::tuple<std::string, ConfigMap> nextResult("", {});
        nextResult = co_await std::get<0>(configIntfAddedRemovedMatches[intf])
                         ->next<std::string, ConfigMap>();

        auto& [objPath, serviceIntfMap] = nextResult;

        debug("detected interface added on {PATH}", "PATH", objPath);

        for (auto& [service, _] : serviceIntfMap)
        {
            co_await handleConfigurationInterfaceFound(service, objPath, intf);
        }
    }
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<void> SoftwareManager::awaitConfigIntfRemovedMatch(
    std::string intf)
// NOLINTEND(readability-static-accessed-through-instance)
{
    if (!configIntfAddedRemovedMatches.contains(intf))
    {
        error("internal map of dbus matches corrupted, missing {INTF}", "INTF",
              intf);
        co_return;
    }

    while (!ctx.stop_requested())
    {
        auto nextResult =
            co_await std::get<1>(configIntfAddedRemovedMatches[intf])
                ->next<std::string, std::vector<std::string>>();

        auto [objPath, serviceIntfMap] = nextResult;

        debug("detected interface removed on {PATH}", "PATH", objPath);

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
                debug(
                    "removal of device at {PATH} ignored because of in-progress update",
                    "PATH", path.str);
                continue;
            }

            deviceToRemove = path;
        }

        if (!deviceToRemove.str.empty())
        {
            debug("removing device at {PATH}", "PATH", deviceToRemove.str);
            devices.erase(deviceToRemove);
        }
    }
}
