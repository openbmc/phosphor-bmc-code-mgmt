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

    static std::string getObjPath(const std::string& swid);
    static std::string getRandomSoftwareId();

    static bool setHostPowerstate(bool state);

    // derived class should override this member
    const std::string serviceName =
        "xyz.openbmc_project.Software.FWUpdate.Undefined";

    bool dryRun;
    sdbusplus::bus_t& bus;
    sdbusplus::async::context& io;

    // set of devices found through configuration and probing
    std::set<std::shared_ptr<Device>> devices;
};
