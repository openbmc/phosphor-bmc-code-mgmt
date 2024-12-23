#pragma once

#include "common/include/device.hpp"
#include "common/include/host_power.hpp"
#include "common/include/software.hpp"
#include "common/include/software_manager.hpp"
#include "eeprom_device_version.hpp"

#include <gpiod.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/bus/match.hpp>

#include <string>

using phosphor::software::config::SoftwareConfig;
using phosphor::software::device::Device;
using phosphor::software::host_power::HostPower;
using phosphor::software::manager::SoftwareManager;

namespace phosphor::software::eeprom
{

class EEPROMDevice : public Device
{
  public:
    EEPROMDevice(sdbusplus::async::context& ctx, const uint8_t& bus,
                 const uint8_t& address, const std::string& chipModel,
                 const std::vector<std::string>& gpioLines,
                 const std::vector<int>& gpioPolarities,
                 std::unique_ptr<DeviceVersion> deviceVersion,
                 SoftwareConfig& config, SoftwareManager* parent);

    using Device::softwareCurrent;

    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) final;

  private:
    uint16_t bus;
    uint8_t address;
    std::string chipModel;
    std::vector<std::string> gpioLines;
    std::vector<int> gpioPolarities;
    std::unique_ptr<DeviceVersion> deviceVersion;
    std::unique_ptr<HostPower> hostPower;

    /**
     * @brief Get the driver path for the EEPROM device.
     *
     * @return std::string
     */
    std::string getDriverPath();
    /**
     * @brief Constructs the I2C device identifier in the format "bus-address".
     *
     * @return The I2C device identifier string.
     */
    std::string getI2CDeviceId() const;
    /**
     * @brief Finds the EEPROM device file path in sysfs.
     *
     * @return The EEPROM device file path, or an empty string if not found.
     */
    std::string getEEPROMPath();
    /**
     * @brief Binds the EEPROM device driver to the I2C device.
     *
     * @return `true` on success, `false` otherwise.
     */
    sdbusplus::async::task<bool> bindEEPROM();
    /**
     * @brief Unbinds the EEPROM device driver from the I2C device.
     *
     * @return `true` on success, `false` otherwise.
     */
    sdbusplus::async::task<bool> unbindEEPROM();
    /**
     * @brief Checks if the EEPROM device is currently bound to its driver.
     *
     * @return `true` if the EEPROM device is bound, `false` otherwise.
     */
    bool isEEPROMBound();
    /**
     * @brief Writes data to the EEPROM device.
     *
     * @param image         - Pointer to the data to write.
     * @param image_size    - Size of the data to write in bytes.
     * @return `true` on success, `false` otherwise.
     */
    sdbusplus::async::task<bool> writeEEPROM(const uint8_t* image,
                                             size_t image_size);
    /**
     * @brief Writes data to the EEPROM device when it is bound to the driver.
     *
     * @param image         - Pointer to the data to write.
     * @param image_size    - Size of the data to write in bytes.
     * @return `true` on success, `false` otherwise.
     */
    sdbusplus::async::task<bool> writeEEPROMGPIOSetDeviceBound(
        const uint8_t* image, size_t image_size);

    /**
     *  @brief Handle async host state change signal and updates the version.
     */
    sdbusplus::async::task<> hostStateChangedUpdateVersion();
};

} // namespace phosphor::software::eeprom
