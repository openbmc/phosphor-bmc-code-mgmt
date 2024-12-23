#include "eeprom_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

#include <filesystem>
#include <fstream>

PHOSPHOR_LOG2_USING;

namespace fs = std::filesystem;

EEPROMDevice::EEPROMDevice(sdbusplus::async::context& ctx, const uint8_t& bus,
                           const uint8_t& address,
                           const std::vector<std::string>& gpioLines,
                           const std::vector<uint8_t>& gpioPolarities,
                           SoftwareConfig& config, SoftwareManager* parent) :
    Device(ctx, config, parent,
           {RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset}),
    bus(bus), address(address), gpioLines(gpioLines),
    gpioPolarities(gpioPolarities)
{
    debug("initialized EEPROM device instance on dbus");
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> EEPROMDevice::updateDevice(const uint8_t* image,
                                                        size_t image_size)
// NOLINTEND(readability-static-accessed-through-instance)
{
    // NOLINTBEGIN(readability-static-accessed-through-instance)
    bool success = co_await this->writeEEPROM(image, image_size);
    // NOLINTEND(readability-static-accessed-through-instance)
    if (!success)
    {
        error("failed to update EEPROM device");
        co_return false;
    }

    info("successfully updated EEPROM device");

    co_return true;
}

std::optional<gpiod::chip> EEPROMDevice::findChipForLine(
    const std::string& lineName)
{
    uint8_t chipIndex = 0;
    while (true)
    {
        try
        {
            gpiod::chip chip("/dev/gpiochip" + std::to_string(chipIndex));
            if (chip.find_line(lineName))
            {
                return chip;
            }
            ++chipIndex;
        }
        catch (const std::exception&)
        {
            break;
        }
    }
    return std::nullopt;
}

bool EEPROMDevice::setGPIOs(const std::vector<std::string>& gpioLines,
                            const std::vector<uint8_t>& gpioPolarities)
{
    if (gpioLines.size() != gpioPolarities.size())
    {
        error("gpioLines and gpioPolarities sizes do not match");
        return false;
    }

    for (size_t i = 0; i < gpioLines.size(); i++)
    {
        auto chipOpt = findChipForLine(gpioLines[i]);
        if (!chipOpt.has_value())
        {
            error("failed to find chip for line: {LINE}", "LINE", gpioLines[i]);
            return false;
        }

        gpiod::chip& chip = *chipOpt;
        gpiod::line line = chip.find_line(gpioLines[i]);
        if (line.is_used())
        {
            error("gpio line {LINE} was still used", "LINE", line.name());
            return false;
        }
        line.request({"", gpiod::line_request::DIRECTION_OUTPUT, 0},
                     gpioPolarities[i]);
    }

    return true;
}

// TODO: do not hardcode the driver path
const std::string eepromDriverPath = "/sys/bus/i2c/drivers/at24";

std::string EEPROMDevice::getI2CDeviceId() const
{
    std::ostringstream oss;
    oss << bus << "-" << std::hex << std::setfill('0') << std::setw(4)
        << static_cast<int>(address);
    return oss.str();
}

std::string EEPROMDevice::getEEPROMPath()
{
    std::string basePath = "/sys/bus/i2c/devices/";
    std::string devicePath = basePath + getI2CDeviceId() + "/eeprom";

    if (fs::exists(devicePath) && fs::is_regular_file(devicePath))
    {
        debug("found EEPROM device path: {PATH}", "PATH", devicePath);
        return devicePath;
    }

    return "";
}
// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> EEPROMDevice::bindEEPROM()
// NOLINTEND(readability-static-accessed-through-instance)
{
    info("binding {DEVICE} EEPROM device", "DEVICE", getI2CDeviceId());
    std::string bindPath = eepromDriverPath + "/bind";
    std::ofstream ofbind(bindPath, std::ofstream::out);
    if (!ofbind)
    {
        error("failed to open bind file: {PATH}", "PATH", bindPath);
        co_return false;
    }

    ofbind << getI2CDeviceId();
    ofbind.close();

    // wait for kernel
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(2));

    co_return isEEPROMBound();
}
// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> EEPROMDevice::unbindEEPROM()
// NOLINTEND(readability-static-accessed-through-instance)
{
    debug("unbinding EEPROM device {DEVICE}", "DEVICE", getI2CDeviceId());
    std::string unbindPath = eepromDriverPath + "/unbind";
    std::ofstream ofunbind(unbindPath, std::ofstream::out);
    if (!ofunbind)
    {
        error("failed to open unbind file: {PATH}", "PATH", unbindPath);
        co_return false;
    }
    ofunbind << getI2CDeviceId();
    ofunbind.close();

    // wait for kernel
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(2));

    co_return !isEEPROMBound();
}

bool EEPROMDevice::isEEPROMBound()
{
    std::string path = eepromDriverPath + "/" + getI2CDeviceId();
    return std::filesystem::exists(path);
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> EEPROMDevice::writeEEPROM(const uint8_t* image,
                                                       size_t image_size)
// NOLINTEND(readability-static-accessed-through-instance)
{
    bool success = true;

    debug("[gpio] requesting gpios to mux EEPROM device to BMC");
    success = setGPIOs(gpioLines, gpioPolarities);
    if (!success)
    {
        error("failed to set GPIOs for EEPROM device");
        co_return success;
    }

    this->setUpdateProgress(20);

    success = co_await writeEEPROMGPIOSet(image, image_size);
    if (success)
    {
        info("successfully wrote EEPROM device");
        this->setUpdateProgress(90);
    }
    else
    {
        error("failed to write EEPROM device");
    }

    std::vector<uint8_t> invertedGpioPolarities;
    invertedGpioPolarities.reserve(gpioPolarities.size());
    std::transform(gpioPolarities.begin(), gpioPolarities.end(),
                   std::back_inserter(invertedGpioPolarities),
                   [](uint8_t value) { return value ^ 1; });

    debug("[gpio] requesting GPIOs to restore EEPROM device state.");
    success = setGPIOs(gpioLines, invertedGpioPolarities);
    if (!success)
    {
        error("failed to restore EEPROM device state");
        co_return success;
    }

    this->setUpdateProgress(100);

    co_return success;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> EEPROMDevice::writeEEPROMGPIOSet(
    const uint8_t* image, size_t image_size)
// NOLINTEND(readability-static-accessed-through-instance)
{
    bool success = true;

    if (isEEPROMBound())
    {
        debug("unbinding already bound EEPROM device");
        success = co_await unbindEEPROM();

        if (!success)
        {
            error("error unbinding spi flash");
            co_return false;
        }
    }

    success = co_await bindEEPROM();
    if (!success)
    {
        error("failed to bind EEPROM device");
        co_await unbindEEPROM();
        co_return false;
    }

    co_await writeEEPROMGPIOSetDeviceBound(image, image_size);

    success = co_await unbindEEPROM();

    co_return success;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> EEPROMDevice::writeEEPROMGPIOSetDeviceBound(
    const uint8_t* image, size_t image_size)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto eepromPath = getEEPROMPath();
    if (eepromPath.empty())
    {
        error("EEPROM file not found for device: {DEVICE}", "DEVICE",
              getI2CDeviceId());
        co_return false;
    }

    std::ofstream eepromFile(eepromPath, std::ios::binary);
    if (!eepromFile)
    {
        error("failed to open EEPROM file: {PATH}", "PATH", eepromPath);
        co_return false;
    }

    debug("writing EEPROM device, image size: {SIZE}", "SIZE", image_size);

    if (image_size >
        static_cast<size_t>(std::numeric_limits<std::streamsize>::max()))
    {
        error("image size {SIZE} exceeds streamsize max limit", "SIZE",
              image_size);
        co_return false;
    }

    eepromFile.write(reinterpret_cast<const char*>(image),
                     static_cast<std::streamsize>(image_size));
    if (!eepromFile)
    {
        error("failed to write data to EEPROM file");
        eepromFile.close();
        co_return false;
    }

    eepromFile.close();

    co_return true;
}
