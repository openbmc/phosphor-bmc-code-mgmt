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
class FWManager;

// represents an arbitrary device which has firmware.
// Some devices can be updated.
class Device
{
  public:
    Device(sdbusplus::async::context& io, bool dryRun, FWManager* parent);

    virtual void startUpdate(sdbusplus::message::unix_fd image,
                             sdbusplus::common::xyz::openbmc_project::software::
                                 ApplyTime::RequestedApplyTimes applyTime,
                             const std::string& oldSwId) = 0;

    virtual void startUpdateAsync() = 0;

    void deleteOldSw(const std::string& swid);

    FWManager* parent;
    bool dryRun;
    sdbusplus::bus_t& bus;
    sdbusplus::async::context& io;

    // members to handle the asynchronous update
    sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
        RequestedApplyTimes applyTime;
    sdbusplus::message::unix_fd image;
    std::string oldSwId; // to be deleted on update completion

    // software instance, identified by their swid
    // The specific derived class also owns its dbus interfaces,
    // which are destroyed when the instance is deleted from this map
    std::shared_ptr<Software> softwareCurrent;
    std::shared_ptr<Software> softwareUpdate;
};
