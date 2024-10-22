#pragma once

#include "common/include/software_manager.hpp"
#include "sdbusplus/async/context.hpp"

class I2CVRDeviceCodeUpdater : public SoftwareManager
{
  public:
    I2CVRDeviceCodeUpdater(sdbusplus::async::context& ctx, bool isDryRun);

    std::set<std::string> getEMConfigTypes() override;

    sdbusplus::async::task<> getInitialConfigurationSingleDevice(
        const std::string& service, DeviceConfig& config) final;
};
