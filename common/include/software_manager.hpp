#pragma once

#include "device.hpp"
#include "sdbusplus/async/proxy.hpp"

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/timer.hpp>

#include <string>

// This is the base class for the code updater
// Every code updater can inherit from this
class SoftwareManager
{
  public:
    SoftwareManager(sdbusplus::async::context& ctx,
                    const std::string& busNameSuffix, bool dryRun);

    // @param state   desired powerstate (true means on)
    // @returns       true on success
    sdbusplus::async::task<bool> setHostPowerstate(bool state);

    // @returns       true when powered
    // @returns       std::nullopt on failure to query power state
    sdbusplus::async::task<std::optional<bool>> getHostPowerstate();

    // Fetches initial configuration from dbus.
    // This should be called once by a code updater at startup.
    // It will call 'getInitialConfigurationSingleDevice' for each device
    // configured
    // @param configurationInterfaces    the dbus interfaces from which to fetch
    // configuration
    sdbusplus::async::task<> getInitialConfiguration(
        const std::vector<std::string>& configurationInterfaces);

    // request the bus name on dbus after all configuration has been parsed
    // and the devices have been initialized
    // @returns        the name on dbus
    std::string setupBusName();

    // set of devices found through configuration and probing
    std::set<std::unique_ptr<Device>> devices;

  protected:
    const bool dryRun;

    template <typename T>
    sdbusplus::async::task<std::optional<T>>
        dbusGetRequiredConfigurationProperty(
            const std::string& service, const std::string& path,
            const std::string& property, SoftwareConfig& config)
    {
        std::string configIface =
            "xyz.openbmc_project.Configuration." + config.configType;
        std::optional<T> res = co_await dbusGetRequiredProperty<T>(
            service, path, configIface, property);
        co_return res;
    }

    // this variant also logs the error
    template <typename T>
    sdbusplus::async::task<std::optional<T>> dbusGetRequiredProperty(
        const std::string& service, const std::string& path,
        const std::string& intf, const std::string& property)
    {
        std::optional<T> opt =
            co_await dbusGetProperty<T>(service, path, intf, property);
        if (!opt.has_value())
        {
            lg2::error(
                "[config] missing property {PROPERTY} on path {PATH}, interface {INTF}",
                "PROPERTY", property, "PATH", path, "INTF", intf);
        }
        co_return opt;
    }

    template <typename T>
    sdbusplus::async::task<std::optional<T>> dbusGetProperty(
        const std::string& service, const std::string& path,
        const std::string& intf, const std::string& property)
    {
        auto client =
            sdbusplus::async::proxy().service(service).path(path).interface(
                "org.freedesktop.DBus.Properties");

        try
        {
            std::variant<T> result = co_await client.call<std::variant<T>>(
                ctx, "Get", intf, property);

            T res = std::get<T>(result);
            co_return res;
        }
        catch (std::exception& e)
        {
            co_return std::nullopt;
        }
    }

    // This function receives a dbus name and object path for a single device,
    // which was configured.
    // The component code updater overrides this function and may create a
    // device instance internally, or reject the configuration as invalid.
    // @param service       The dbus name where our configuration is
    // @param config        The common configuration properties which are shared
    // by all devices.
    //                      Also includes the object path to fetch other
    //                      configuration properties.
    virtual sdbusplus::async::task<> getInitialConfigurationSingleDevice(
        const std::string& service, const std::string& path,
        SoftwareConfig& config) = 0;

    sdbusplus::async::context& ctx;

  private:
    // this is appended to the common prefix to construct the dbus name
    std::string busNameSuffix;

    sdbusplus::server::manager_t manager;

    friend Software;
    friend Device;
};
