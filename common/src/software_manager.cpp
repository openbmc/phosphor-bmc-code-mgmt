#include "software_manager.hpp"

#include "dbus_helper.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

#include <cstdint>

PHOSPHOR_LOG2_USING;

SoftwareManager::SoftwareManager(sdbusplus::async::context& ctx,
                                 const std::string& serviceNameSuffix) :
    ctx(ctx), serviceNameSuffix(serviceNameSuffix), manager(ctx, "/")
{
    debug("initialized SoftwareManager");
}

std::string SoftwareManager::setupBusName()
{
    const std::string serviceNameFull =
        "xyz.openbmc_project.Software." + serviceNameSuffix;

    debug("requesting dbus name {BUSNAME}", "BUSNAME", serviceNameFull);

    ctx.get_bus().request_name(serviceNameFull.c_str());

    return serviceNameFull;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<> SoftwareManager::initDevices(
    const std::vector<std::string>& configurationInterfaces)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto client = sdbusplus::client::xyz::openbmc_project::ObjectMapper<>(ctx)
                      .service("xyz.openbmc_project.ObjectMapper")
                      .path("/xyz/openbmc_project/object_mapper");

    auto res =
        co_await client.get_sub_tree("/xyz/openbmc_project/inventory", 0, {});

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

            debug(
                "[config] found configuration interface at {SERVICE}, {OBJPATH}",
                "SERVICE", service, "OBJPATH", path);

            std::optional<uint64_t> vendorIANA =
                co_await dbusGetRequiredProperty<uint64_t>(
                    ctx, service, path, interfaceFound, "VendorIANA");

            std::optional<std::string> compatible =
                co_await dbusGetRequiredProperty<std::string>(
                    ctx, service, path, interfaceFound, "Compatible");

            std::optional<std::string> emConfigType =
                co_await dbusGetRequiredProperty<std::string>(
                    ctx, service, path, interfaceFound, "Type");

            std::optional<std::string> emConfigName =
                co_await dbusGetRequiredProperty<std::string>(
                    ctx, service, path, interfaceFound, "Name");

            if (!vendorIANA.has_value() || !compatible.has_value() ||
                !emConfigType.has_value() || !emConfigName.has_value())
            {
                continue;
            }

            SoftwareConfig config(path, vendorIANA.value(), compatible.value(),
                                  emConfigType.value(), emConfigName.value());

            co_await initDevice(service, path, config);
        }
    }

    debug("[config] done with initial configuration");

    setupBusName();
}
