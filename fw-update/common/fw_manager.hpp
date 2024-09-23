#pragma once

#include "device.hpp"

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/timer.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

#include <string>

// This is the base class for the code updater
// Every code updater can inherit from this
class FWManager
{
  public:
    FWManager(sdbusplus::async::context& io, const std::string& serviceName,
              bool dryRun);

    virtual bool verifyImage(sdbusplus::message::unix_fd image) = 0;

    // this is the actual update function, which is run asynchronously
    // virtual void startUpdateAsync() = 0;

    static std::string getObjPathFromSwid(const std::string& swid);
    static long int getRandomId();
    static std::string getRandomSoftwareId();

    static bool setHostPowerstate(bool state);

    void requestBusName();

    bool dryRun;
    std::string serviceName;
    sdbusplus::bus_t& bus;
    sdbusplus::async::context& io;

    // set of devices found through configuration and probing
    std::set<std::shared_ptr<Device>> devices;

    // the dbus interface we use for configuration
    virtual std::string getConfigurationInterface() = 0;
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
};
