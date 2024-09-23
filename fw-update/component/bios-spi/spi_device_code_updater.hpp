#pragma once

#include "fw-update/common/include/software_manager.hpp"
#include "sdbusplus/async/context.hpp"

#include <sdbusplus/async.hpp>

class SPIDeviceCodeUpdater : public SoftwareManager
{
  public:
    SPIDeviceCodeUpdater(sdbusplus::async::context& io, bool isDryRun,
                         bool debug);

    // misc
    auto getInitialConfiguration() -> sdbusplus::async::task<>;
    void getInitialConfigurationSingleDevice(
        const std::string& service, const std::string& path, size_t ngpios);

  private:
};
