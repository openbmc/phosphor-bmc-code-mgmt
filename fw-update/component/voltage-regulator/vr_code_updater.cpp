#include "vr_code_updater.hpp"

#include <phosphor-logging/lg2.hpp>

VRCodeUpdater::VRCodeUpdater(const std::shared_ptr<sdbusplus::asio::connection>& conn, boost::asio::io_context& io, bool isDryRun): CodeUpdater(conn, io, "VR", isDryRun)
{
	lg2::info("VR Code Updater started");
	//TODO
}

bool VRCodeUpdater::verifyImage(sdbusplus::message::unix_fd image)
{
	lg2::info("VR Code Updater: verify image");
	(void)image;
	//TODO
	return true;
}

void VRCodeUpdater::startUpdateAsync(const boost::system::error_code& error){
	lg2::info("VR Code Updater: async update function");
	(void)error;
	//TODO
}

void VRCodeUpdater::writeToVR(uint32_t bus, uint8_t addr, const uint8_t* fw, size_t fw_size)
{
	(void)bus;
	(void)addr;
	(void)fw;
	(void)fw_size;
	//TODO
	lg2::info("VR Code Updater: write to VR");
}
