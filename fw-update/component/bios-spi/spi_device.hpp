#pragma once

#include "fw-update/common/device.hpp"
#include "fw-update/common/fw_manager.hpp"

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

class SPIDevice : public Device
{
  public:
    SPIDevice(sdbusplus::async::context& io, const std::string& serviceName,
              bool dryRun, FWManager* parent);

    void startUpdate(sdbusplus::message::unix_fd image,
                     sdbusplus::common::xyz::openbmc_project::software::
                         ApplyTime::RequestedApplyTimes applyTime,
                     const std::string& oldSwId) final;

    void continueUpdate(sdbusplus::message::unix_fd image,
                        sdbusplus::common::xyz::openbmc_project::software::
                            ApplyTime::RequestedApplyTimes applyTime,
                        const std::string& oldSwId);

    void startUpdateAsync() final;

    // SPI specific members and functions
    std::string spiDev;

    static void bindSPIFlash(const std::string& spi_dev);
    static void unbindSPIFlash(const std::string& spi_dev);
    void writeSPIFlash(sdbusplus::message::unix_fd image,
                       const std::string& lineName);
};
