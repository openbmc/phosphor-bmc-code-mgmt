#pragma once

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

class Software;

// This is the base class for the code updater
// Every code updater can inherit from this
class FWManager
{
  public:
    FWManager(sdbusplus::async::context& io, const std::string& serviceName,
              bool dryRun);

    void startUpdate(sdbusplus::message::unix_fd image,
                     sdbusplus::common::xyz::openbmc_project::software::
                         ApplyTime::RequestedApplyTimes applyTime,
                     const std::string& oldSwId);

    void deleteOldSw(const std::string& swid);

    virtual bool verifyImage(sdbusplus::message::unix_fd image) = 0;

    // this is the actual update function, which is run asynchronously
    virtual void startUpdateAsync() = 0;

    static std::string getObjPath(const std::string& swid);
    static std::string getRandomSoftwareId();

    static bool setHostPowerstate(bool state);

    // derived class should override this member
    const std::string serviceName =
        "xyz.openbmc_project.Software.FWUpdate.Undefined";

    bool dryRun;
    sdbusplus::bus_t& bus;

    // members to handle the asynchronous update
    // TODO: replace this with some other timer
    // boost::asio::steady_timer updateTimer;
    sdbusplus::Timer updateTimer;

    sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
        RequestedApplyTimes applyTime;
    sdbusplus::message::unix_fd image;
    std::string oldSwId; // to be deleted on update completion

    // maps swid to software instance.
    // The specific derived class also owns its dbus interfaces,
    // which are destroyed when the instance is deleted from this map
    std::map<std::string, std::shared_ptr<Software>> softwares;
};
