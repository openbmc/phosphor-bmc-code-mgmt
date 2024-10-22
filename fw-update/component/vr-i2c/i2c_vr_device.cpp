#include "i2c_vr_device.hpp"

#include "fw-update/common/device.hpp"
#include "fw-update/common/fw_manager.hpp"
#include "vr_fw.hpp"

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/timer.hpp>

using namespace std::literals;

I2CVRDevice::I2CVRDevice(sdbusplus::async::context& io, bool dryRun,
                         const uint32_t& busNo, const uint32_t& addr,
                         const std::string& vendorIANA,
                         const std::string& compatible, FWManager* parent) :
    Device(io, dryRun, vendorIANA, compatible, parent), busNo(busNo),
    address(addr)
{
    lg2::debug("initialized I2C device instance on dbus");
}
// NOLINTBEGIN
sdbusplus::async::task<bool> I2CVRDevice::startUpdateAsync()
// NOLINTEND
{
    lg2::debug("starting async update");

    std::string newSwid = FWManager::getRandomSoftwareId();
    std::string objPathStr = FWManager::getObjPathFromSwid(newSwid);
    const char* objPath = objPathStr.c_str();

    std::shared_ptr<VRFW> newvrsw =
        std::make_shared<VRFW>(parent->io, bus, newSwid, objPath, this);

    softwareUpdate = newvrsw;

    bool success = co_await Device::continueUpdate(image, applyTime, oldSwId);

    if (success)
    {
        softwareCurrent = newvrsw;
    }

    co_return success;
}

// NOLINTBEGIN
sdbusplus::async::task<bool> I2CVRDevice::deviceSpecificUpdateFunction(
    sdbusplus::message::unix_fd image,
    std::shared_ptr<SoftwareActivationProgress> activationProgress)
// NOLINTEND
{
    // NOLINTBEGIN
    bool success = co_await this->writeUpdate(image, activationProgress);
    // NOLINTEND
    co_return success;
}

// NOLINTBEGIN
sdbusplus::async::task<bool> I2CVRDevice::writeUpdate(
    sdbusplus::message::unix_fd image,
    const std::shared_ptr<SoftwareActivationProgress>& activationProgress)
// NOLINTEND
{
    // Not required to power cycle the host, just copied bios_spi here.
    // NOLINTBEGIN
    bool success = co_await parent->setHostPowerstate(false);
    // NOLINTEND
    if (!success)
    {
        lg2::error("error changing host power state off");
        co_return false;
    }

    // Update loop, contition is size of image and reduced by size written to
    // chip.
    while (0)
    {
        // acticationProgress->progress(1...100);
        (void)image;
        (void)activationProgress;
    }

    // Not required to power cycle the host, just copied bios_spi here.
    success = co_await parent->setHostPowerstate(true);

    if (!success)
    {
        lg2::error("error changing host power state on");
        co_return false;
    }
    co_return true;
}
