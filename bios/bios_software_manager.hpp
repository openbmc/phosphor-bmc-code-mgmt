#pragma once

#include "common/include/software_manager.hpp"

#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>

using namespace phosphor::software::manager;

const std::string configTypeBIOS = "BIOS";

class BIOSSoftwareManager : public SoftwareManager
{
  public:
    BIOSSoftwareManager(sdbusplus::async::context& ctx, bool isDryRun);

    sdbusplus::async::task<bool> initDevice(const std::string& service,
                                            const std::string& path,
                                            SoftwareConfig& config) final;

  private:
    bool dryRun;
};
