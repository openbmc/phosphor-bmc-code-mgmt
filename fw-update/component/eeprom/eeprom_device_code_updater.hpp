#pragma once

#include "fw-update/common/include/software_manager.hpp"
#include "sdbusplus/async/context.hpp"

#include <sdbusplus/async.hpp>

class EEPROMDeviceCodeUpdater : public SoftwareManager
{
  public:
    EEPROMDeviceCodeUpdater(sdbusplus::async::context& ctx, bool isDryRun);

    std::set<std::string> getEMConfigTypes() override;
    sdbusplus::async::task<> getInitialConfigurationSingleDevice(
        const std::string& service, DeviceConfig& config) final;

  private:
};
