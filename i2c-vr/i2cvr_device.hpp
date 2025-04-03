#pragma once

#include "common/include/device.hpp"
#include "common/include/software_config.hpp"
#include "common/include/software_manager.hpp"
#include "vr.hpp"

namespace SoftwareInf = phosphor::software;
namespace ManagerInf = SoftwareInf::manager;
namespace DeviceInf = SoftwareInf::device;
namespace ConfigInf = SoftwareInf::config;

namespace VRInf = SoftwareInf::VR;

namespace SDBusPlusSoftware = sdbusplus::common::xyz::openbmc_project::software;

namespace phosphor::software::i2c_vr::device
{

class I2CVRDevice : public DeviceInf::Device
{
  public:
    using DeviceInf::Device::softwareCurrent;
    I2CVRDevice(sdbusplus::async::context& ctx, enum VRInf::VRType vrType,
                const uint16_t& bus, const uint8_t& address,
                ConfigInf::SoftwareConfig& config,
                ManagerInf::SoftwareManager* parent) :
        DeviceInf::Device(
            ctx, config, parent,
            {SDBusPlusSoftware::ApplyTime::RequestedApplyTimes::Immediate,
             SDBusPlusSoftware::ApplyTime::RequestedApplyTimes::OnReset}),
        vrInterface(VRInf::create(ctx, vrType, bus, address))
    {}

    std::unique_ptr<VRInf::VoltageRegulator> vrInterface;

    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) final;

    sdbusplus::async::task<bool> getVersion(uint32_t* sum) const;
};

} // namespace phosphor::software::i2c_vr::device
