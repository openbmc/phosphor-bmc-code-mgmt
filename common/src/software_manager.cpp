#include "software_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/bus.hpp>
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
