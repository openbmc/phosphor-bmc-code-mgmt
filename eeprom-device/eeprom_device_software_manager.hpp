#pragma once

#include "common/include/software_manager.hpp"

namespace ManagerInf = phosphor::software::manager;

const std::string configTypeEEPROMDevice = "EEPROMDevice";

class EEPROMDeviceSoftwareManager : public ManagerInf::SoftwareManager
{
  public:
    EEPROMDeviceSoftwareManager(sdbusplus::async::context& ctx) :
        SoftwareManager(ctx, configTypeEEPROMDevice)
    {}

    void start();

    sdbusplus::async::task<bool> initDevice(const std::string& service,
                                            const std::string& path,
                                            SoftwareConfig& config) final;

  private:
    sdbusplus::async::task<bool> getDeviceProperties(
        const std::string& service, const std::string& path,
        const std::string& intf, uint16_t& bus, uint8_t& address,
        std::string& chipModel);
};
