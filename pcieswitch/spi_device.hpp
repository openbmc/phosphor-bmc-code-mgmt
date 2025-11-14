#pragma once

#include "common/include/device.hpp"
#include "common/include/software.hpp"
#include "common/include/software_manager.hpp"
#include "common/include/host_power.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>

#include <string>

class SPIDevice;

using namespace phosphor::software;
using namespace phosphor::software::manager;
using namespace phosphor::software::host_power;

const std::string versionUnknown = "Unknown";

const std::string rbbPCIeSwitchSysPath = "/sys/bus/usb/devices/2-2.4.6/idProduct";
const std::string lbbPCIeSwitchSysPath = "/sys/bus/usb/devices/2-2.5.6/idProduct";
const std::string amcPCIeSwitchSysPath = "/sys/bus/usb/devices/2-2.2/idProduct";
const std::string ft4222DID = "601c";

enum FT4222
{
    LBB = 0,
    RBB,
    AMC,
    FT4222_END
};

class SPIDevice : public Device
{
  public:
    using Device::softwareCurrent;
    SPIDevice(sdbusplus::async::context& ctx, std::string& chipName,
              const std::vector<std::string>& gpioLines,
              const std::vector<std::string>& gpioValues, SoftwareConfig& config,
              SoftwareManager* parent) :
    Device(ctx, config, parent,
           {RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset}),
    gpioLines(gpioLines), gpioValues(gpioValues), chipName(chipName)
    {}

    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t imageSize) final;
    sdbusplus::async::task<bool> getVersion(std::string& version);
    void getFt4222AdapterNum(bool* devExist, std::string& adapterNum);

  private:
    std::vector<std::string> gpioLines;
    std::vector<std::string> gpioValues;
    std::string chipName;
};
