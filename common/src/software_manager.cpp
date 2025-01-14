#include "software_manager.hpp"

#include "sdbusplus/async/timer.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

SoftwareManager::SoftwareManager(sdbusplus::async::context& ctx,
                                 const std::string& busNameSuffix,
                                 bool isDryRun) :
    dryRun(isDryRun), ctx(ctx), busNameSuffix(busNameSuffix), manager(ctx, "/")
{
    lg2::debug("initialized SoftwareManager");
}

std::string SoftwareManager::setupBusName()
{
    const std::string serviceNameFull =
        "xyz.openbmc_project.Software." + this->busNameSuffix;

    lg2::debug("requesting dbus name {BUSNAME}", "BUSNAME", serviceNameFull);

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
        lg2::debug("[config] looking for dbus interface {INTF}", "INTF", iface);
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

            lg2::debug(
                "[config] found configuration interface at {SERVICE}, {OBJPATH}",
                "SERVICE", service, "OBJPATH", path);

            std::optional<uint64_t> optVendorIANA =
                co_await SoftwareManager::dbusGetRequiredProperty<uint64_t>(
                    service, path, interfaceFound, "VendorIANA");

            std::optional<std::string> optCompatible =
                co_await SoftwareManager::dbusGetRequiredProperty<std::string>(
                    service, path, interfaceFound, "Compatible");

            std::optional<std::string> optEMConfigType =
                co_await SoftwareManager::dbusGetRequiredProperty<std::string>(
                    service, path, interfaceFound, "Type");

            std::optional<std::string> optEMConfigName =
                co_await SoftwareManager::dbusGetRequiredProperty<std::string>(
                    service, path, interfaceFound, "Name");

            if (!optVendorIANA.has_value() || !optCompatible.has_value() ||
                !optEMConfigType.has_value() || !optEMConfigName.has_value())
            {
                continue;
            }

            SoftwareConfig config(
                path, optVendorIANA.value(), optCompatible.value(),
                optEMConfigType.value(), optEMConfigName.value());

            co_await initSingleDevice(service, path, config);
        }
    }

    lg2::debug("[config] done with initial configuration");

    setupBusName();
}
