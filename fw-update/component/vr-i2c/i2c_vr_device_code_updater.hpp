#pragma once

#include "fw-update/common/include/software_manager.hpp"
#include "sdbusplus/async/context.hpp"

#include <sdbusplus/async.hpp>

class I2CVRDeviceCodeUpdater : public SoftwareManager
{
  public:
    I2CVRDeviceCodeUpdater(sdbusplus::async::context& ctx, bool isDryRun,
                           bool debug);

    std::set<std::string> getEMConfigTypes() override;

    sdbusplus::async::task<> getInitialConfigurationSingleDevice(
        const std::string& service, DeviceConfig& config) final;

    bool debug;

  private:
};
