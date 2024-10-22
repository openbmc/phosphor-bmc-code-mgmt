#pragma once

#include "common/include/software_manager.hpp"

#include <sdbusplus/async/context.hpp>

using namespace phosphor::software::manager;

const std::string configTypeVRDevice = "XDPE1X2XX";

class I2CVRSoftwareManager : public SoftwareManager
{
  public:
    I2CVRSoftwareManager(sdbusplus::async::context& ctx);

    sdbusplus::async::task<> initDevice(
        const std::string& service, const std::string& path,
        SoftwareConfig& config) final;

    bool dryRun;
};
