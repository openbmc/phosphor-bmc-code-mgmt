#pragma once

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

#include <string>

// This is the code updater interface.
// Every code updater follows this interface
class ICodeUpdater
{
  public:
    virtual bool verifyImage(sdbusplus::message::unix_fd image) = 0;
    virtual void deleteOldSw(const std::string& swid) = 0;

    // The synchronous start update function, called from the dbus interface
    // of the currently running software, which provides it access to the old
    // software id. This function should start the timer to run the async update
    // function.
    virtual void startUpdate(sdbusplus::message::unix_fd image,
                             sdbusplus::common::xyz::openbmc_project::software::
                                 ApplyTime::RequestedApplyTimes applyTime,
                             const std::string& oldSwId) = 0;

    // this is the actual update function, which is run asynchronously
    virtual void startUpdateAsync(const boost::system::error_code& error) = 0;
};

class Software;

// This is the base class for the code updater
// Every code updater can inherit from this
class CodeUpdater : ICodeUpdater
{
  public:
    CodeUpdater(const std::shared_ptr<sdbusplus::asio::connection>& bus,
                boost::asio::io_context& io, const std::string& serviceName,
                bool dryRun);

    void startUpdate(sdbusplus::message::unix_fd image,
                     sdbusplus::common::xyz::openbmc_project::software::
                         ApplyTime::RequestedApplyTimes applyTime,
                     const std::string& oldSwId) final;

    void deleteOldSw(const std::string& swid) final;

    static std::string getObjPath(const std::string& swid);
    static std::string getRandomSoftwareId();

    static bool setHostPowerstate(bool state);

    // derived class should override this member
    const std::string serviceName =
        "xyz.openbmc_project.Software.FWUpdate.Undefined";

    bool dryRun;
    const std::shared_ptr<sdbusplus::asio::connection>& bus;
    sdbusplus::asio::object_server objServer;

    // members to handle the asynchronous update
    boost::asio::steady_timer updateTimer;
    sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
        RequestedApplyTimes applyTime;
    sdbusplus::message::unix_fd image;
    std::string oldSwId; // to be deleted on update completion

    // maps swid to software instance.
    // The specific derived class also owns its dbus interfaces,
    // which are destroyed when the instance is deleted from this map
    std::map<std::string, std::shared_ptr<Software>> softwares;
};
