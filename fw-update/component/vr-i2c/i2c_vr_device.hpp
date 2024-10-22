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
    I2CVRDevice(sdbusplus::async::context& io, bool dryRun, const uint32_t& bus,
                const std::string& vrChipName,
				const uint32_t& address, const std::string& vendorIANA,
                const std::string& compatible, FWManager* parent);

    sdbusplus::async::task<bool> startUpdateAsync() final;

    sdbusplus::async::task<bool> deviceSpecificUpdateFunction(
    	const uint8_t* image, size_t image_size,
        std::shared_ptr<SoftwareActivationProgress> activationProgress) final;

  private:
	// busNo represents an I2C bus number, read from Entity-Manager
    uint32_t busNo;
	// address represents an I2C slave address, read from Entity-Manager
    uint32_t address;

	// Placeholder for VoltageRegulator hardware interface in change-75657
	// VoltageRegulator* inf;

    // this function writes the update image to the device on I2C Bus busNo,
	// on address device.
	sdbusplus::async::task<bool> writeUpdate(
        const uint8_t* image, size_t image_size,
        const std::shared_ptr<SoftwareActivationProgress>& activationProgreess);

};
