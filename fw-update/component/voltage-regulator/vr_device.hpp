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

    sdbusplus::async::task<bool> startUpdateAsync() final;

    sdbusplus::async::task<bool> deviceSpecificUpdateFunction(
        sdbusplus::message::unix_fd image,
        std::shared_ptr<SoftwareActivationProgress> activationProgress) final;

    void deleteOldSw(const std::string& swid);
};
