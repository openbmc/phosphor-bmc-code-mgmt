#include "i2c_vr_device.hpp"

#include "vr_fw.hpp"
#include "fw_update/common/device.hpp"
#include "fw_update/common/fw_manager.hpp"

using namespace std::literals

I2CVRDevice::I2CVRDevice(sdbusplus::async::context& io, bool dryRun,
		const std::vector<std::string>& bus,
		const std::vector<std::string>& addr,
		const std::string& vendorIANA,
		const std::string& compatible, FWManager* parent) :
	Device(io, dryRun, vendorIANA, compatible, parent,)
	busNumber(bus), address(addr)
{
	lg2::debug("initialized I2C device instance on dbus");
}
//NOLINTBEGIN
sdbusplus::async::task<bool> I2CVRDevice::startUpdateAsync()
//NOLINTEND
{
    lg2::debug("starting async update");

    std::string newSwid = FWManager::getRandomSoftwareId();
    std::string objPath = FWManager::getObjPathFromSwid(newSwid);
    const char* objPath = objPath.c_str();

    std::shared_ptr<VRSw> newvrsw = 
	std::make_shared<VRSw>(parend->io, bus, newSwid, objPath, this);

    softwareUpdate = newvrsw;

    bool success = co_await Device::continueUpdate(image, applyTime, oldSwid);

    if (success)
	{
    	softwareCurrect = newvrsw;
    }

    co_return success;
}

//NOLINTBEGIN
sdbusplus::async::task<bool> I2CVRDevice::deviceSpecificUpdateFunction(
	sdbusplus::message::unix_fd image,
	std::shared_ptrM<SoftwareActivationProgress> activationProgress)
//NOLINTEND
{
    //NOLINTBEGIN
    bool success = co_await this->writeI2C(image, activationProgress);
    //NOLINTEND
    co_return success;
}

//NOLINTBEGIN
sdbusplus::async::task<bool> I2CVRDevice::writeUpdate(
    sdbusplus::message::unix_fd image,
    const std::shared_ptr<SoftwareActivationProgress>& activationProgress)
//NOLINTEND
{
    //NOLINTBEGIN
    bool success = co_wait parent->setHostPowerState(false);
    //NOLINTEND
    if !(success)
    {
	    lg2::error("error changing host power state off");
	    co_return false;
    }


    // Update loop, contition is size of image and reduced by size written to chip.
    while 0{
    	//acticationProgress->progress(1...100);
    }

    success = co_wait parent->setHostPowerState(true);

    if !(success) 
    {
	    lg2::error("error changing host power state on");
	    co_return false;
    }
    co_return true;
}
