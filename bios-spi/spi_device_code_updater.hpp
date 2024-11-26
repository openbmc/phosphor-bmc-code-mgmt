#pragma once

#include "common/include/software_manager.hpp"
#include "sdbusplus/async/context.hpp"

#include <sdbusplus/async.hpp>

const std::string configTypeSPIDevice = "SPIFlash";

class SPIDeviceCodeUpdater : public SoftwareManager
{
  public:
    SPIDeviceCodeUpdater(sdbusplus::async::context& ctx, bool isDryRun,
                         bool debug);

    sdbusplus::async::task<> initDevice(const std::string& service,
                                        const std::string& path,
                                        SoftwareConfig& config) final;

    bool dryRun;
    bool debug;

  private:
};
