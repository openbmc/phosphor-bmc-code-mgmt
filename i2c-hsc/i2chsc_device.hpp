#pragma once

#include "common/include/device.hpp"
#include "common/include/software_config.hpp"
#include "common/include/software_manager.hpp"
#include "hsc.hpp"

namespace SoftwareInf = phosphor::software;
namespace ManagerInf = SoftwareInf::manager;
namespace DeviceInf = SoftwareInf::device;
namespace ConfigInf = SoftwareInf::config;

namespace HSCInf = SoftwareInf::HSC;

namespace SDBusPlusSoftware = sdbusplus::common::xyz::openbmc_project::software;

namespace phosphor::software::i2c_hsc::device
{

class I2CHSCDevice : public DeviceInf::Device
{
  public:
    using DeviceInf::Device::softwareCurrent;
    I2CHSCDevice(sdbusplus::async::context& ctx, enum HSCInf::HSCType hscType,
                 const uint16_t& bus, const uint8_t& address,
                 ConfigInf::SoftwareConfig& config,
                 ManagerInf::SoftwareManager* parent) :
        DeviceInf::Device(
            ctx, config, parent,
            {SDBusPlusSoftware::ApplyTime::RequestedApplyTimes::OnReset}),
        hscInterface(HSCInf::create(ctx, hscType, bus, address))
    {}

    std::unique_ptr<HSCInf::HotSwapController> hscInterface;

    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) final;

    sdbusplus::async::task<bool> getVersion(uint32_t* sum) const;
};

} // namespace phosphor::software::i2c_hsc::device
