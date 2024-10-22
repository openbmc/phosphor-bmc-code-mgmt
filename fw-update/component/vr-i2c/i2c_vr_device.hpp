#pragma once

#include "fw-update/common/device.hpp"
#include "fw-update/common/fw_manager.hpp"

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/timer.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

#include <string>

class Software;

class I2CVRDevice : public Device
{
	public:
		I2CVRDevice(sdbusplus::async::context& io, bool dryRun,
			const uint32_t& bus, const uint32_t& address,
			const std::string& vendorIANA, const std::string& compatible,
			FWManager* parent);

		sdbusplus::async::task<bool> startUpdateAsync() final;

		sdbusplus::async::task<bool> deviceSpecificUpdateFunction(
			sdbusplus::message::unix_fd image,
			std::shared_ptr<SoftwareActivationProgress> activationProgress) final;

	private:
		uint32_t busNo;
		uint32_t address;

		sdbusplus::async::task<bool> writeUpdate( 
			sdbusplus::message::unix_fd image, 
			const std::shared_ptr<SoftwareActivationProgress>& activationProgreess);

		// Does the host need to be powered off?
		// Do we need function handling that?
};
