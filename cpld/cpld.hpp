#pragma once

#include "common/include/device.hpp"
#include "common/include/software_manager.hpp"
#include "cpld_interface.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software;
using namespace phosphor::software::manager;
namespace CPLDInf = phosphor::software::cpld;

namespace phosphor::software::cpld::device
{

class CPLDDevice : public Device
{
  public:
    CPLDDevice(sdbusplus::async::context& ctx, const std::string& vendor,
               const std::string& chipname, const uint16_t& bus,
               const uint8_t& address, SoftwareConfig& config,
               SoftwareManager* parent) :
        Device(ctx, config, parent, {RequestedApplyTimes::Immediate}),
        cpldInterface(CPLDInf::CPLDFactory::instance().create(
            vendor, chipname, bus, address))
    {}

    using Device::softwareCurrent;
    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) final;

  private:
    std::unique_ptr<CPLDInf::CPLDInterface> cpldInterface;
};

} // namespace phosphor::software::cpld::device
