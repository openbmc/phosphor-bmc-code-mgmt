#pragma once

#include "common/include/software_manager.hpp"

using namespace phosphor::software::manager;

const std::string configTypeEEPROMDevice = "EEPROMDeviceFirmware";

class EEPROMDeviceCodeUpdater : public SoftwareManager
{
  public:
    EEPROMDeviceCodeUpdater(sdbusplus::async::context& ctx);

    sdbusplus::async::task<> initDevice(const std::string& service,
                                        const std::string& path,
                                        SoftwareConfig& config) final;
};
