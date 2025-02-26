#pragma once

#include "common/include/software_manager.hpp"

using namespace phosphor::software::manager;

namespace phosphor::software::cpld
{

class CPLDSoftwareManager : public SoftwareManager
{
  public:
    CPLDSoftwareManager(sdbusplus::async::context& ctx) :
        SoftwareManager(ctx, "CPLDFirmware")
    {}

    sdbusplus::async::task<bool> initDevice(const std::string& service,
                                            const std::string& path,
                                            SoftwareConfig& config) final;

    void start();
};

} // namespace phosphor::software::cpld
