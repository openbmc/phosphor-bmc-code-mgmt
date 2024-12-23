#include "eeprom_device.hpp"

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

EEPROMDevice::EEPROMDevice(sdbusplus::async::context& ctx, bool dryRun,
                           const uint8_t& bus, const uint8_t& address,
                           const std::vector<MuxGpioConfig>& muxGpioConfigs,
                           DeviceConfig& config, SoftwareManager* parent) :
    Device(ctx, dryRun, config, parent), busNo(bus), address(address),
    muxGpioConfigs(muxGpioConfigs)
{
    lg2::debug("initialized EEPROM Device instance on dbus");
}
// NOLINTBEGIN
sdbusplus::async::task<std::string> EEPROMDevice::getInventoryItemObjectPath()
// NOLINTEND
{
    co_return this->config.objPathConfig;
}
// NOLINTBEGIN
sdbusplus::async::task<bool> EEPROMDevice::deviceSpecificUpdateFunction(
    const uint8_t* image, size_t image_size,
    std::unique_ptr<SoftwareActivationProgress>& activationProgress)
// NOLINTEND
{
    // NOLINTBEGIN
    bool success = co_await this->writeEEPROM(image, image_size);
    if (!success)
    {
        lg2::error("error writing EEPROM");
        co_return false;
    }
    activationProgress->progress(100);

    // NOLINTEND
    co_return success;
}

// TODO: do not hardcode the driver path
const std::string eepromDriverPath = "/sys/bus/i2c/drivers/at24";

std::string EEPROMDevice::getI2CDeviceId()
{
    std::ostringstream oss;
    oss << busNo << "-" << std::hex << std::setfill('0') << std::setw(4)
        << static_cast<int>(address);
    return oss.str();
}

std::string EEPROMDevice::getEepromPath()
{
    const std::string basePath = "/sys/bus/i2c/devices/";
    const std::string devicePath = basePath + getI2CDeviceId() + "/eeprom";

    if (fs::exists(devicePath) && fs::is_regular_file(devicePath))
    {
        lg2::debug("[EEPROM] found EEPROM path: {PATH}", "PATH", devicePath);
        return devicePath;
    }

    return "";
}
// NOLINTBEGIN
sdbusplus::async::task<bool> EEPROMDevice::bindEEPROM()
// NOLINTEND
{
    lg2::info("[EEPROM] binding {DEVICE} EEPROM", "DEVICE", getI2CDeviceId());
    std::string bindPath = eepromDriverPath + "/bind";
    std::ofstream ofbind(bindPath, std::ofstream::out);
    if (!ofbind)
    {
        lg2::error("Failed to open bind file: {PATH}", "PATH", bindPath);
        co_return false;
    }

    ofbind << getI2CDeviceId();
    ofbind.close();

    // wait for kernel
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(2));

    co_return isEepromBound();
}
// NOLINTBEGIN
sdbusplus::async::task<bool> EEPROMDevice::unbindEEPROM()
// NOLINTEND
{
    lg2::info("[EEPROM] unbinding {DEVICE} EEPROM", "DEVICE", getI2CDeviceId());
    std::string unbindPath = eepromDriverPath + "/unbind";
    std::ofstream ofunbind(unbindPath, std::ofstream::out);
    if (!ofunbind)
    {
        lg2::error("Failed to open unbind file: {PATH}", "PATH", unbindPath);
        co_return false;
    }
    ofunbind << getI2CDeviceId();
    ofunbind.close();

    // wait for kernel
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(2));

    co_return !isEepromBound();
}

bool EEPROMDevice::isEepromBound()
{
    std::string path = eepromDriverPath + "/" + getI2CDeviceId();
    return std::filesystem::exists(path);
}

sdbusplus::async::task<bool>
    EEPROMDevice::writeEEPROM(const uint8_t* image, size_t image_size)
{
    bool success = true;
    std::vector<MuxGpioConfig> invertedMuxGpioConfigs;

    lg2::debug("[gpio] requesting gpios to mux EEPROM to BMC");
    for (const auto& muxGpioConfig : muxGpioConfigs)
    {
        gpiod::chip chip;
        try
        {
            chip = gpiod::chip(muxGpioConfig.chipName);
        }
        catch (std::exception& e)
        {
            lg2::error(e.what());
            co_return false;
        }
        gpiod::line line = chip.find_line(muxGpioConfig.lineName);
        if (line.is_used())
        {
            lg2::error("gpio line {LINE} was still used", "LINE", line.name());
            co_return false;
        }
        invertedMuxGpioConfigs.emplace_back(muxGpioConfig.chipName,
                                            muxGpioConfig.lineName,
                                            muxGpioConfig.value ? 0 : 1);
        line.request({"", gpiod::line_request::DIRECTION_OUTPUT, 0},
                     muxGpioConfig.value);
    }

    success = co_await writeEepromGPIOSet(image, image_size);

    lg2::debug("[gpio] requesting gpios to mux EEPROM to previous state");
    for (const auto& invertedMuxGpioConfig : invertedMuxGpioConfigs)
    {
        gpiod::chip chip;
        try
        {
            chip = gpiod::chip(invertedMuxGpioConfig.chipName);
        }
        catch (std::exception& e)
        {
            lg2::error(e.what());
            co_return false;
        }
        gpiod::line line = chip.find_line(invertedMuxGpioConfig.lineName);
        line.request({"", gpiod::line_request::DIRECTION_OUTPUT,
                      invertedMuxGpioConfig.value});

        line.release();
    }

    if (success)
    {
        lg2::info("[EEPROM] successfully wrote EEPROM");
    }
    else
    {
        lg2::error("[EEPROM] error writing EEPROM");
    }

    co_return true;
}

sdbusplus::async::task<bool>
    EEPROMDevice::writeEepromGPIOSet(const uint8_t* image, size_t image_size)
{
    bool success = true;

    if (isEepromBound())
    {
        lg2::debug("[EEPROM] eeprom was already bound, unbinding it now");
        success = co_await unbindEEPROM();

        if (!success)
        {
            lg2::error("[EEPROM] error unbinding spi flash");
            co_return false;
        }
    }

    success = co_await bindEEPROM();

    if (!success)
    {
        lg2::error("[EEPROM] failed to bind eeprom device");
        co_await unbindEEPROM();
        co_return false;
    }

    if (dryRun)
    {
        lg2::info("[EEPROM] dry run, NOT writing to the eeprom");
    }
    else
    {
        auto eepromPath = getEepromPath();
        if (eepromPath.empty())
        {
            lg2::error("[EEPROM] EEPROM file not found for device: {DEVICE}",
                       "DEVICE", getI2CDeviceId());
            success = false;
        }
        else
        {
            std::ofstream eepromFile(eepromPath, std::ios::binary);
            if (!eepromFile)
            {
                lg2::error("Failed to open EEPROM file: {PATH}", "PATH",
                           eepromPath);
                success = false;
            }
            else
            {
                lg2::debug("[EEPROM] writing EEPROM, image size: {SIZE}",
                           "SIZE", image_size);
                eepromFile.write(reinterpret_cast<const char*>(image),
                                 image_size);
                eepromFile.close();
                success = true;
            }
        }
    }

    co_await unbindEEPROM();

    co_return success;
}
