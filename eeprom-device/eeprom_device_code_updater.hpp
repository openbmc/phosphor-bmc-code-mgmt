#pragma once

#include "common/include/software_manager.hpp"

using namespace phosphor::software::manager;

const std::string configTypeEEPROMDevice = "EEPROMDeviceFirmware";

namespace phosphor::software::eeprom
{

class EEPROMDeviceCodeUpdater : public SoftwareManager
{
  public:
    EEPROMDeviceCodeUpdater(sdbusplus::async::context& ctx) :
        SoftwareManager(ctx, configTypeEEPROMDevice)
    {}

    sdbusplus::async::task<> initDevice(const std::string& service,
                                        const std::string& path,
                                        SoftwareConfig& config) final;
};

} // namespace phosphor::software::eeprom
