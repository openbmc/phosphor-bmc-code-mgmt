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
                    const std::string& serviceNameSuffix);

    // Fetches initial configuration from dbus and initializes devices.
    // This should be called once by a code updater at startup.
    // @param configurationInterfaces    the dbus interfaces from which to fetch
    // configuration
    sdbusplus::async::task<> initDevices(
        const std::vector<std::string>& configurationInterfaces);

    // Map of EM config object path to device.
    std::map<sdbusplus::message::object_path, std::unique_ptr<Device>> devices;

  protected:
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
    virtual sdbusplus::async::task<> initSingleDevice(
        const std::string& service, const std::string& path,
        SoftwareConfig& config) = 0;

    sdbusplus::async::context& ctx;

  private:
    // request the bus name on dbus after all configuration has been parsed
    // and the devices have been initialized
    // @returns        the name on dbus
    std::string setupBusName();

    // this is appended to the common prefix to construct the dbus name
    std::string serviceNameSuffix;

    sdbusplus::server::manager_t manager;

    friend Software;
    friend Device;
};
