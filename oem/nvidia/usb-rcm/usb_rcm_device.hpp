#pragma once

#include "common/include/device.hpp"
#include "common/include/software_config.hpp"
#include "common/include/software_manager.hpp"
#include "usb_rcm_driver.hpp"

#include <sdbusplus/async/context.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace SoftwareInf = phosphor::software;
namespace ManagerInf = SoftwareInf::manager;
namespace DeviceInf = SoftwareInf::device;
namespace ConfigInf = SoftwareInf::config;

namespace SDBusPlusSoftware = sdbusplus::common::xyz::openbmc_project::software;

namespace phosphor::software::usb_rcm::device
{

// One NVIDIA Vera CPU recovered over USB RCM. Each entity-manager
// USBRCMFirmware config maps to one device (and one Software.Update object). A
// recovery package carries the CPU's firmware components; they are streamed to
// the RCM bootrom in package (RCM flash) order.
class USBRCMRecoveryDevice : public DeviceInf::Device
{
  public:
    using DeviceInf::Device::softwareCurrent;

    USBRCMRecoveryDevice(sdbusplus::async::context& ctx,
                         DeviceConfig driverConfig,
                         ConfigInf::SoftwareConfig& config,
                         ManagerInf::SoftwareManager* parent) :
        DeviceInf::Device(
            ctx, config, parent,
            {SDBusPlusSoftware::ApplyTime::RequestedApplyTimes::Immediate}),
        driverConfig(std::move(driverConfig))
    {}

    // Single-component packages stream the one image; the default is to consume
    // every applicable component (a Vera recovery package has several).
    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) final;
    sdbusplus::async::task<bool> updateDeviceComponents(
        const std::vector<DeviceInf::ComponentImage>& components) final;

  private:
    DeviceConfig driverConfig;
};

} // namespace phosphor::software::usb_rcm::device
