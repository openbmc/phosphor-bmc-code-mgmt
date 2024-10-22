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
						 const std::string& vrChipName,
                         const uint32_t& bus, const uint32_t& addr,
                         const std::string& vendorIANA,
                         const std::string& compatible, FWManager* parent) :
    Device(io, dryRun, vendorIANA, compatible, parent), busNo(bus),
    address(addr)
{
	// Placeholder code to be adapted in change-75656
	// Create I2CInterface with bus and address
    // Call VoltageRegulator constructor with vrChipName (change-75657).
	lg2::debug(
        "initialized I2C device(on I2C bus: {BUS} with address: {ADDR} instance on dbus",
        "BUS", busNo, "ADDR", address);
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
    const uint8_t* image, size_t image_size,
	std::shared_ptr<SoftwareActivationProgress> activationProgress)
// NOLINTEND
{
    // NOLINTBEGIN
    bool success = co_await this->writeUpdate(image, image_size,
										activationProgresss);
    // NOLINTEND
    co_return success;
}

// NOLINTBEGIN
sdbusplus::async::task<bool> I2CVRDevice::writeUpdate(
    const uint8_t* image, size_t image_size,
	std::shared_ptr<SoftwareActivationProgress> activationProgress)
// NOLINTEND
{
    // Update loop, contition is size of image and reduced by size written to
    // chip.
    while (0)
    {
        // acticationProgress->progress(1...100);
        (void)image;
        (void)activationProgress;
    }

	co_return true;
}
