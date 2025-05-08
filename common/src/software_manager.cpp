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
static sdbusplus::async::task<std::optional<SoftwareConfig>> getConfig(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& objectPath, const std::string& interfacePrefix)
// NOLINTEND(readability-static-accessed-through-instance)
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

            debug(
                "[config] found configuration interface at {SERVICE}, {OBJPATH}",
                "SERVICE", service, "OBJPATH", path);

            auto optConfig =
                co_await getConfig(ctx, service, path, interfaceFound);
            if (!optConfig.has_value())
            {
                error("Error fetching common configuration from {PATH}", "PATH",
                      path);
                continue;
            }

            co_await initDevice(service, path, optConfig.value());
        }
    }

    debug("Done with initial configuration");
}
