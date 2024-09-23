#pragma once

#include "fw-update/common/fw_manager.hpp"
#include "sdbusplus/async/context.hpp"

#include <boost/asio/steady_timer.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

class SPIDeviceCodeUpdater : public FWManager
{
  public:
    SPIDeviceCodeUpdater(sdbusplus::async::context& io, bool isDryRun);

    // override code updater functions

    bool verifyImage(sdbusplus::message::unix_fd image) override;

    // misc
    auto getInitialConfiguration() -> sdbusplus::async::task<>;
    void getInitialConfigurationSingleDevice(const std::string& service,
                                             const std::string& path);

  private:
};
