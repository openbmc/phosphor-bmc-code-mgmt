#pragma once

#include "common/include/device.hpp"

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
                 SoftwareConfig& config, SoftwareManager* parent);

    using Device::softwareCurrent;

    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) final;

  private:
    bool dryRun;
    uint16_t busNo;
    uint8_t address;

    std::vector<MuxGpioConfig> muxGpioConfigs;
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
    std::string getEepromPath();
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
     * @brief Sets the required GPIO states before writing to the EEPROM device
     *        and performs the write operation.
     *
     * @param image         - Pointer to the data to write.
     * @param image_size    - Size of the data to write in bytes.
     * @return `true` on success, `false` otherwise.
     */
    sdbusplus::async::task<bool> writeEEPROMGPIOSet(const uint8_t* image,
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
};
