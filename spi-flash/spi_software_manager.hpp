#pragma once

#include "common/include/software_manager.hpp"

#include <sdbusplus/async.hpp>

namespace phosphor::software::manager
{

class SPISoftwareManager : public SoftwareManager
{
  public:
    SPISoftwareManager(sdbusplus::async::context& ctx, bool isDryRun);

    sdbusplus::async::task<bool> initDevice(const std::string& service,
                                            const sdbusplus::object_path& path,
                                            SoftwareConfig& config) final;

    void start();

  private:
    bool dryRun;
};

} // namespace phosphor::software::manager
