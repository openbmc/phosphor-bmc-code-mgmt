#pragma once

#include "common/include/software_manager.hpp"

using namespace phosphor::software::manager;

const std::string configTypeCPLD = "CPLDFirmware";

namespace phosphor::software::cpld
{

class CPLDCodeUpdater : public SoftwareManager
{
  public:
    CPLDCodeUpdater(sdbusplus::async::context& ctx) :
        SoftwareManager(ctx, configTypeCPLD) {}

    sdbusplus::async::task<bool> initDevice(const std::string& service,
                                        const std::string& path,
                                        SoftwareConfig& config) final;
};

} // namespace phosphor::software::cpld
