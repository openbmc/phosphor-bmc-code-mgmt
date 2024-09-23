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

void VRDevice::startUpdateAsync() {}

void VRDevice::deviceSpecificUpdateFunction(sdbusplus::message::unix_fd image)
{
    (void)image;
    // TODO
}
