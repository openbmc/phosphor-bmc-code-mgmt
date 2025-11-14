#pragma once

#include "common/include/software_manager.hpp"

#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>

using namespace phosphor::software::manager;

const std::string configTypePCIESWITCH = "PCIESWITCH";

class PCIESWITCHSoftwareManager : public SoftwareManager
{
  public:
    PCIESWITCHSoftwareManager(sdbusplus::async::context& ctx) :
        SoftwareManager(ctx, configTypePCIESWITCH)
    {}
    sdbusplus::async::task<bool> initDevice(const std::string& service,
                                            const std::string& path,
                                            SoftwareConfig& config) final;
};
