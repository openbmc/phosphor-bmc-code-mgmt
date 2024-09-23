#pragma once

#include "device.hpp"

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/timer.hpp>

#include <string>

// This is the base class for the code updater
// Every code updater can inherit from this
class FWManager
{
  public:
    FWManager(sdbusplus::async::context& io, const std::string& configType,
              bool dryRun, bool debug);

    // @param state   desired powerstate (true means on)
    // @returns       true on success
    sdbusplus::async::task<bool> setHostPowerstate(bool state);

    // @returns       true when powered
    // @returns       std::nullopt on failure to query power state
    sdbusplus::async::task<std::optional<bool>> getHostPowerstate();

    // request the bus name on dbus after all configuration has been parsed
    // and the devices have been initialized
    void requestBusName();

    sdbusplus::async::context& getIOContext();

    const bool dryRun;
    const bool debug;

    // set of devices found through configuration and probing
    std::set<std::unique_ptr<Device>> devices;

    // e.g. "SPIFlash", "CPLD", ...
    // This is the 'Type' field in the EM exposes record
    const std::string configType;

  protected:
    // the dbus interface we use for configuration
    virtual std::string getConfigurationInterface() final;
    std::string getConfigurationInterfaceMuxGpios();

    template <typename T>
    std::optional<T> dbusGetRequiredConfigurationProperty(
        const std::string& service, const std::string& path,
        const std::string& property)
    {
        return dbusGetRequiredProperty<T>(
            service, path, getConfigurationInterface(), property);
    }

    // this variant also logs the error
    template <typename T>
    std::optional<T> dbusGetRequiredProperty(
        const std::string& service, const std::string& path,
        const std::string& intf, const std::string& property)
    {
        std::optional<T> opt =
            dbusGetProperty<T>(service, path, intf, property);
        if (!opt.has_value())
        {
            lg2::error(
                "[config] missing property {PROPERTY} on path {PATH}, interface {INTF}",
                "PROPERTY", property, "PATH", path, "INTF", intf);
        }
        return opt;
    }

    template <typename T>
    std::optional<T>
        dbusGetProperty(const std::string& service, const std::string& path,
                        const std::string& intf, const std::string& property)
    {
        auto m = bus.new_method_call(service.c_str(), path.c_str(),
                                     "org.freedesktop.DBus.Properties", "Get");
        m.append(intf.c_str(), property.c_str());

        try
        {
            auto reply = m.call();

            std::variant<T> vpath;
            reply.read(vpath);

            T res = std::get<T>(vpath);
            return res;
        }
        catch (std::exception& e)
        {
            return std::nullopt;
        }
    }

  private:
    sdbusplus::bus_t& bus;
    sdbusplus::async::context& io;

    sdbusplus::server::manager_t manager;
};
