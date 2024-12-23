#pragma once

#include "common/include/software_manager.hpp"

const std::string configTypeEEPROMDevice = "EEPROMDevice";

class EEPROMDeviceCodeUpdater : public SoftwareManager
{
  public:
    EEPROMDeviceCodeUpdater(sdbusplus::async::context& ctx, bool dryRun);

    sdbusplus::async::task<> initSingleDevice(const std::string& service,
                                              const std::string& path,
                                              SoftwareConfig& config) final;

    bool dryRun;
};
