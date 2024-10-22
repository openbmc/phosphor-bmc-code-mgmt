#pragma once

#include "fw-update/common/include/device.hpp"
#include "fw-update/common/include/software_manager.hpp"

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>

#include <string>

class I2CVRDevice : public Device
{
  public:
    I2CVRDevice(sdbusplus::async::context& ctx, bool dryRun,
                const std::string& vrChipType, const uint16_t& bus,
                const uint8_t& address, DeviceConfig& config,
                SoftwareManager* parent, bool debug);

    sdbusplus::async::task<bool> deviceSpecificUpdateFunction(
        const uint8_t* image, size_t image_size,
        std::unique_ptr<SoftwareActivationProgress>& activationProgress) final;

	sdbusplus::async::task<std::string> getInventoryItemObjectPath() final;
  private:
    // busNo represents an I2C bus number, read from Entity-Manager
    uint16_t busNo;
    // address represents an I2C slave address, read from Entity-Manager
    uint8_t address;
    // vrChipType represents an voltage regulator device type spicific string
    // read from Entity-Manager
    std::string vrTypeStr;

    bool debug;

    // Placeholder for VoltageRegulator hardware interface in change-75657
    // VoltageRegulator* inf;

    // this function writes the update image to the device on I2C Bus busNo,
    // on address device.
    sdbusplus::async::task<bool> writeUpdate(
        const uint8_t* image, size_t image_size,
        const std::unique_ptr<SoftwareActivationProgress>& activationProgreess);
};
