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

VRDevice::VRDevice(sdbusplus::async::context& io,
                   const std::string& serviceName, bool dryRun,
                   FWManager* parent) : Device(io, serviceName, dryRun, parent)
{
    lg2::debug("initialized SPI Device instance on dbus");
}

void VRDevice::startUpdate(sdbusplus::message::unix_fd image,
                           sdbusplus::common::xyz::openbmc_project::software::
                               ApplyTime::RequestedApplyTimes applyTime,
                           const std::string& oldSwId)
{
    (void)applyTime;
    (void)oldSwId;
    (void)image;
    /*
    const int status = verifyPLDMPackage(image);
    if (status != 0)
    {
        return;
    }
    */

    /*
    this->image = image;
    this->applyTime = applyTime;
    this->oldSwId = oldSwId;
    */

    const int timeout = 7;
    lg2::debug("starting timer for async bios update in {SECONDS}s\n",
               "SECONDS", timeout);

    updateTimer.start(std::chrono::seconds(timeout));
}

void VRDevice::startUpdateAsync() {}
