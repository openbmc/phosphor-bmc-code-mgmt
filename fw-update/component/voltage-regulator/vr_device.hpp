#pragma once

#include "fw-update/common/device.hpp"

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

class VRDevice : public Device
{
  public:
    VRDevice(sdbusplus::async::context& io, bool dryRun, FWManager* parent);

    void startUpdate(sdbusplus::message::unix_fd image,
                     sdbusplus::common::xyz::openbmc_project::software::
                         ApplyTime::RequestedApplyTimes applyTime,
                     const std::string& oldSwId) final;

    void continueUpdate(sdbusplus::message::unix_fd image,
                        sdbusplus::common::xyz::openbmc_project::software::
                            ApplyTime::RequestedApplyTimes applyTime,
                        const std::string& oldSwId, const std::string& newSwId);

    void startUpdateAsync() final;

    void deleteOldSw(const std::string& swid);
};
