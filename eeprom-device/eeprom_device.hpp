#pragma once

#include "common/include/device.hpp"
#include "common/include/software.hpp"
#include "common/include/software_manager.hpp"

#include <gpiod.hpp>
#include <sdbusplus/async/context.hpp>

#include <string>

using namespace phosphor::software;
using namespace phosphor::software::manager;

namespace phosphor::software::eeprom::device
{

class EEPROMDevice : public Device
{
  public:
    EEPROMDevice(sdbusplus::async::context& ctx, const uint8_t& bus,
                 const uint8_t& address, const std::string& chipModel,
                 const std::vector<std::string>& gpioLines,
                 const std::vector<uint8_t>& gpioPolarities,
                 SoftwareConfig& config, SoftwareManager* parent);

    using Device::softwareCurrent;

    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) final;

  private:
    uint16_t bus;
    uint8_t address;
    std::string chipModel;
    std::vector<std::string> gpioLines;
    std::vector<uint8_t> gpioPolarities;

    /**
     * @brief Finds the GPIO chip associated with a given GPIO line name.
     *
     * @return The GPIO chip that contains the specified line name, or an empty.
     */
    static std::optional<gpiod::chip> findChipForLine(
        const std::string& lineName);
    /**
     * @brief Sets the state of multiple GPIO lines.
     *
     * @return `true` if all GPIOs were successfully set, `false` otherwise.
     */
    static bool setGPIOs(const std::vector<std::string>& gpioLines,
                         const std::vector<uint8_t>& gpioPolarities);
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

} // namespace phosphor::software::eeprom::device
