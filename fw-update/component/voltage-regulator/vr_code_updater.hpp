#pragma once

#include "fw-update/common/code_updater.hpp"

// code updater for voltage regulator
class VRCodeUpdater : CodeUpdater
{
public:
	VRCodeUpdater(const std::shared_ptr<sdbusplus::asio::connection>& conn, boost::asio::io_context& io, bool isDryRun);

	// override CodeUpdater functions

	bool verifyImage(sdbusplus::message::unix_fd image) override;
	void startUpdateAsync(const boost::system::error_code& error) override;

	// VR specific functions
	static void writeToVR(uint32_t bus, uint8_t addr, const uint8_t* fw, size_t fw_size);
};
