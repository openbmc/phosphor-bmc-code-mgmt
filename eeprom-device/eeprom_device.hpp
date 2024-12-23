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

namespace SoftwareInf = phosphor::software;
namespace ManagerInf = SoftwareInf::manager;
namespace HostPowerInf = SoftwareInf::host_power;

class EEPROMDevice : public Device
{
  public:
    EEPROMDevice(sdbusplus::async::context& ctx, uint16_t bus, uint8_t address,
                 const std::string& chipModel,
                 const std::vector<std::string>& gpioLines,
                 const std::vector<bool>& gpioPolarities,
                 std::unique_ptr<DeviceVersion> deviceVersion,
                 SoftwareConfig& config, ManagerInf::SoftwareManager* parent);

    using Device::softwareCurrent;

    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) final;

  private:
    uint16_t bus;
    uint8_t address;
    std::string chipModel;
    std::vector<std::string> gpioLines;
    std::vector<bool> gpioPolarities;
    std::unique_ptr<DeviceVersion> deviceVersion;
    HostPowerInf::HostPower hostPower;

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
     * @brief Writes data to the EEPROM.
     *
     * @param image         - Pointer to the data to write.
     * @param image_size    - Size of the data to write in bytes.
     * @return `true` on success, `false` otherwise.
     */
    sdbusplus::async::task<int> writeEEPROM(const uint8_t* image,
                                            size_t image_size) const;
    /**
     *  @brief Handle async host state change signal and updates the version.
     */
    sdbusplus::async::task<> processHostStateChange();
};
