#pragma once

#include "fw-update/common/code_updater.hpp"

#include <boost/asio/steady_timer.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

class SPIDeviceCodeUpdater : public CodeUpdater
{
  public:
    SPIDeviceCodeUpdater(
        const std::shared_ptr<sdbusplus::asio::connection>& conn,
        boost::asio::io_context& io, bool isDryRun);

    // override code updater functions

    bool verifyImage(sdbusplus::message::unix_fd image) override;

    void startUpdateAsync(const boost::system::error_code& error) override;

    void continueUpdate(sdbusplus::message::unix_fd image,
                        sdbusplus::common::xyz::openbmc_project::software::
                            ApplyTime::RequestedApplyTimes applyTime,
                        const std::string& oldSwId, const std::string& newSwId);

    // SPI specific functions
    static void bindSPIFlash(const std::string& spi_dev);
    static void unbindSPIFlash(const std::string& spi_dev);
    void writeSPIFlash(sdbusplus::message::unix_fd image);

  private:
};
