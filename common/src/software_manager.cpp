#include "software_manager.hpp"

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

using namespace phosphor::software::manager;

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

            auto client =
                sdbusplus::async::proxy().service(service).path(path).interface(
                    "org.freedesktop.DBus.Properties");

            uint64_t vendorIANA = 0;
            std::string compatible;
            std::string emConfigType;
            std::string emConfigName;

            try
            {
                {
                    auto propVendorIANA =
                        co_await client.call<std::variant<uint64_t>>(
                            ctx, "Get", interfaceFound, "VendorIANA");

                    vendorIANA = std::get<uint64_t>(propVendorIANA);
                }
                {
                    auto propCompatible =
                        co_await client.call<std::variant<std::string>>(
                            ctx, "Get", interfaceFound, "Compatible");

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
                lg2::error(e.what());
                continue;
            }

            SoftwareConfig config(path, vendorIANA, compatible, emConfigType,
                                  emConfigName);

            co_await initDevice(service, path, config);
        }
    }

    debug("[config] done with initial configuration");

    setupBusName();
}
