#pragma once

#include "common/include/software_manager.hpp"

namespace phosphor::software::cpld
{

class CPLDSoftwareManager : public phosphor::software::manager::SoftwareManager
{
  public:
    CPLDSoftwareManager(sdbusplus::async::context& ctx) :
        SoftwareManager(ctx, "CPLD")
    {}

    sdbusplus::async::task<bool> initDevice(const std::string& service,
                                            const std::string& path,
                                            SoftwareConfig& config) final;

    void start();
};

} // namespace phosphor::software::cpld
