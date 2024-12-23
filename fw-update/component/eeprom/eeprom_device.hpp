#pragma once

#include "fw-update/common/include/device.hpp"
#include "fw-update/common/include/software_manager.hpp"

#include <sdbusplus/async/context.hpp>

#include <string>

class Software;

class MuxGpioConfig
{
  public:
    MuxGpioConfig(const std::string& chipName, const std::string& lineName,
                  const int& value) :
        chipName(chipName), lineName(lineName), value(value)
    {}
    std::string chipName;
    std::string lineName;
    int value;
};

class EEPROMDevice : public Device
{
  public:
    EEPROMDevice(sdbusplus::async::context& ctx, bool dryRun,
                 const uint8_t& bus, const uint8_t& address,
                 const std::vector<MuxGpioConfig>& muxGpioConfigs,
                 DeviceConfig& config, SoftwareManager* parent);

    sdbusplus::async::task<bool> deviceSpecificUpdateFunction(
        const uint8_t* image, size_t image_size,
        std::unique_ptr<SoftwareActivationProgress>& activationProgress) final;

    sdbusplus::async::task<std::string> getInventoryItemObjectPath() final;

  private:
    uint16_t busNo;
    uint8_t address;

    std::vector<MuxGpioConfig> muxGpioConfigs;
    /**
     * @brief Constructs the I2C device identifier in the format "bus-address".
     *
     * @return The I2C device identifier string.
     */
    std::string getI2CDeviceId();
    /**
     * @brief Locates the path to the EEPROM file in the sysfs file system.
     *
     * @return The path to the EEPROM file, or an empty string if not found.
     */
    std::string getEepromPath();
    /**
     * @brief Binds the EEPROM driver to the I2C device.
     *
     * @return `true` on success, `false` otherwise.
     */
    sdbusplus::async::task<bool> bindEEPROM();
    /**
     * @brief Unbinds the EEPROM driver from the I2C device.
     *
     * @return `true` on success, `false` otherwise.
     */
    sdbusplus::async::task<bool> unbindEEPROM();
    /**
     * @brief Checks if the EEPROM device is currently bound to its driver.
     *
     * @return `true` if the EEPROM is bound, `false` otherwise.
     */
    bool isEepromBound();
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
     * @brief Sets the required GPIO states before writing to the EEPROM and
     *        performs the write operation.
     *
     * @param image         - Pointer to the data to write.
     * @param image_size    - Size of the data to write in bytes.
     * @return `true` on success, `false` otherwise.
     */
    sdbusplus::async::task<bool>
        writeEepromGPIOSet(const uint8_t* image, size_t image_size);
};
