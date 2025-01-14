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

// NOLINTBEGIN
sdbusplus::async::task<bool> SoftwareManager::setHostPowerstate(bool state)
// NOLINTEND
{
    auto proxy = sdbusplus::async::proxy()
                     .service("xyz.openbmc_project.State.Host")
                     .path("/xyz/openbmc_project/state/host0")
                     .interface("xyz.openbmc_project.State.Host");

    lg2::info("[PWR] changing host power state to {STATE}", "STATE",
              (state) ? "ON" : "OFF");

    std::string voff = "xyz.openbmc_project.State.Host.Transition.Off";
    std::string von = "xyz.openbmc_project.State.Host.Transition.On";
    std::string targetState;
    if (state)
    {
        co_await proxy.set_property(ctx, "RequestedHostTransition", von);
        targetState = "xyz.openbmc_project.State.Host.HostState.Running";
    }
    else
    {
        co_await proxy.set_property(ctx, "RequestedHostTransition", voff);
        targetState = "xyz.openbmc_project.State.Host.HostState.Off";
    }

    lg2::debug("[PWR] requested host transition to {STATE}", "STATE",
               targetState);

    lg2::debug("[PWR] async sleep to wait for state transition");
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(10));

    auto actualOpt = co_await getHostPowerstate();

    if (actualOpt == std::nullopt)
    {
        co_return false;
    }

    const bool actual = actualOpt.value();

    if (actual == state)
    {
        lg2::debug("[PWR] successfully achieved state {STATE}", "STATE",
                   targetState);
        co_return true;
    }
    else
    {
        lg2::debug("[PWR] failed to achieve state {STATE}", "STATE",
                   targetState);
        co_return false;
    }
}

// NOLINTBEGIN
sdbusplus::async::task<std::optional<bool>> SoftwareManager::getHostPowerstate()
// NOLINTEND
{
    auto proxy = sdbusplus::async::proxy()
                     .service("xyz.openbmc_project.State.Host")
                     .path("/xyz/openbmc_project/state/host0")
                     .interface("xyz.openbmc_project.State.Host");

    std::string stateOn = "xyz.openbmc_project.State.Host.HostState.Running";
    std::string stateOff = "xyz.openbmc_project.State.Host.HostState.Off";

    std::string res =
        co_await proxy.get_property<std::string>(ctx, "CurrentHostState");

    if (res == stateOn)
    {
        co_return true;
    }
    else if (res == stateOff)
    {
        co_return false;
    }

    lg2::error("[PWR] unexpected power state: {STATE}", "STATE", res);

    co_return true;
}

// NOLINTBEGIN
sdbusplus::async::task<> SoftwareManager::getInitialConfiguration(
    const std::vector<std::string>& configurationInterfaces)
// NOLINTEND
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

            SoftwareConfig config(optVendorIANA.value(), optCompatible.value(),
                                  optEMConfigType.value(),
                                  optEMConfigName.value());

            co_await getInitialConfigurationSingleDevice(service, path, config);
        }
    }

    lg2::debug("[config] done with initial configuration");

    setupBusName();
}
