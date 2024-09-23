#include "vr_device.hpp"

// #include "vr_sw.hpp"
#include "fw-update/common/device.hpp"
#include "fw-update/common/fw_manager.hpp"

#include <boost/asio/steady_timer.hpp>
#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/timer.hpp>

#include <fstream>

VRDevice::VRDevice(sdbusplus::async::context& io, bool dryRun,
                   FWManager* parent) :
    Device(io, dryRun, "TODO", "TODO", parent)
{
    lg2::debug("initialized SPI Device instance on dbus");
}

sdbusplus::async::task<bool> VRDevice::startUpdateAsync()
{
    co_return false;
}

sdbusplus::async::task<bool> VRDevice::deviceSpecificUpdateFunction(
    sdbusplus::message::unix_fd image,
    std::shared_ptr<SoftwareActivationProgress> activationProgress)
{
    activationProgress->progress(10);
    (void)image;
    // TODO
    co_return false;
}
