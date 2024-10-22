#pragma once

#include "fw-update/common/fw_manager.hpp"
#include "sdbusplus/async/context.hpp"

class I2CVRDeviceCodeUpdater : public FWManager
{
  public:
    I2CVRDeviceCodeUpdater(sdbusplus::async::context& io, bool isDryRun);

    bool verifyImage(sdbusplus::message::unix_fd image) override;

    auto getInitialConfiguration() -> sdbusplus::async::task<>;
    void getInitialConfigurationSingleDevice(
      const std::string& service, const std::string& path);

    std::string getConfigurationInterface() override;
}