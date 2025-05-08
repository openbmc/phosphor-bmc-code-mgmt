#pragma once

#include "device.hpp"
#include "sdbusplus/async/match.hpp"

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/timer.hpp>

#include <string>

using namespace phosphor::software::config;
using namespace phosphor::software::device;

namespace phosphor::software::manager
{

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
    // This function receives a dbus name and object path for a single device,
    // which was configured.
    // The component code updater overrides this function and may create a
    // device instance internally, or reject the configuration as invalid.
    // @param service       The dbus name where our configuration is
    // @param config        The common configuration properties which are shared
    // by all devices.
    //                      Also includes the object path to fetch other
    //                      configuration properties.
    // @returns true        if the configuration was accepted
    virtual sdbusplus::async::task<bool> initDevice(const std::string& service,
                                                    const std::string& path,
                                                    SoftwareConfig& config) = 0;

    sdbusplus::async::context& ctx;

  private:
    static sdbusplus::async::task<std::optional<SoftwareConfig>>
        fetchSoftwareConfig(sdbusplus::async::context& ctx,
                            const std::string& service,
                            const std::string& objectPath,
                            const std::string& interfaceFound);

    sdbusplus::async::task<void> handleConfigurationInterfaceFound(
        const std::string& service, const std::string& path,
        const std::string& interface);

    sdbusplus::async::task<void> initConfigIntfMatches(
        const std::vector<std::string>& configInterfaces);

    sdbusplus::async::task<void> awaitConfigIntfAddedMatch(
        std::vector<std::string> interfaces);
    sdbusplus::async::task<void> awaitConfigIntfRemovedMatch(
        std::vector<std::string> interfaces);

    // DBus matches for interfaces added and interfaces removed
    std::unique_ptr<sdbusplus::async::match> configIntfAddedMatch;
    std::unique_ptr<sdbusplus::async::match> configIntfRemovedMatch;

    // this is appended to the common prefix to construct the dbus name
    std::string serviceNameSuffix;

    sdbusplus::server::manager_t manager;

    friend Software;
    friend Device;
};

}; // namespace phosphor::software::manager
