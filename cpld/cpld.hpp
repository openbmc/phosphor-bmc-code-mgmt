#pragma once

#include "common/include/device.hpp"
#include "common/include/software_manager.hpp"
#include "cpld_interface.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>

PHOSPHOR_LOG2_USING;

namespace ManagerInf = phosphor::software::manager;

namespace phosphor::software::cpld
{

class CPLDDevice : public Device
{
  public:
    CPLDDevice(sdbusplus::async::context& ctx, const std::string& chiptype,
               const std::string& chipname, const uint16_t& bus,
               const uint8_t& address, const std::string& hardwareCompatible,
               SoftwareConfig& config, ManagerInf::SoftwareManager* parent) :
        Device(ctx, config, parent,
               {RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset}),
        cpldInterface(CPLDFactory::instance().create(
            chiptype, ctx, chipname, bus, address, hardwareCompatible))
    {}

    using Device::softwareCurrent;
    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) final;
    sdbusplus::async::task<bool> getVersion(std::string& version);

  private:
    std::unique_ptr<CPLDInterface> cpldInterface;
};

} // namespace phosphor::software::cpld
