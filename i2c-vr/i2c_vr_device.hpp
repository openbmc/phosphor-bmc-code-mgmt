#pragma once

#include "common/include/device.hpp"
#include "common/include/software_manager.hpp"
#include "vr.hpp"

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>

using namespace phosphor::software;
using namespace phosphor::software::manager;

namespace phosphor::software::i2c_vr::device
{

class I2CVRDevice : public Device
{
  public:
      using Device::softwareCurrent;
    I2CVRDevice(sdbusplus::async::context& ctx,
            enum VRType vrType, const uint16_t& bus,
                const uint8_t& address, SoftwareConfig& config,
                SoftwareManager* parent):
        Device(ctx, config, parent,
            {RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset}),
    vrInterface(create(vrType, bus, address)){}

    sdbusplus::async::task<bool> updateDevice(const uint8_t* image, size_t image_size) final;

  private:
    std::unique_ptr<VoltageRegulator> vrInterface;
};
}
