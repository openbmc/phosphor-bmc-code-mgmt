#include "eeprom_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

#include <filesystem>
#include <fstream>

PHOSPHOR_LOG2_USING;

namespace fs = std::filesystem;
namespace MatchRules = sdbusplus::bus::match::rules;

using namespace phosphor::software::eeprom::device;

EEPROMDevice::EEPROMDevice(
    sdbusplus::async::context& ctx, const uint8_t& bus, const uint8_t& address,
    const std::string& chipModel, const std::vector<std::string>& gpioLines,
    const std::vector<uint8_t>& gpioPolarities,
    std::unique_ptr<VersionProvider> versionProvider, SoftwareConfig& config,
    SoftwareManager* parent) :
    Device(ctx, config, parent,
           {RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset}),
    bus(bus), address(address), chipModel(chipModel), gpioLines(gpioLines),
    gpioPolarities(gpioPolarities), versionProvider(std::move(versionProvider))
{
    // Some EEPROM devices require the host state to be "on" before retrieving
    // the version. To handle this, set up a match to listen for property
    // changes on the host state, so can update the version once the host state
    // is in the correct condition.
    hostStateMatch = std::make_unique<sdbusplus::bus::match_t>(
        ctx.get_bus(),
        MatchRules::propertiesChanged("/xyz/openbmc_project/state/host0",
                                      "xyz.openbmc_project.State.Host"),
        std::bind(std::mem_fn(&EEPROMDevice::hostStateChangedUpdateVersion),
                  this, std::placeholders::_1));

    debug("Initialized EEPROM device instance on dbus");
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
        error("Failed to update EEPROM device");
        co_return false;
    }

    info("EERPOM device successfully updated");

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
        error("The sizes of gpioLines and gpioPolarities do not match");
        return false;
    }

    for (size_t i = 0; i < gpioLines.size(); i++)
    {
        auto chipOpt = findChipForLine(gpioLines[i]);
        if (!chipOpt.has_value())
        {
            error("Failed to find chip for line: {LINE}", "LINE", gpioLines[i]);
            return false;
        }

        gpiod::chip& chip = *chipOpt;
        gpiod::line line = chip.find_line(gpioLines[i]);
        if (line.is_used())
        {
            error("GPIO line {LINE} was still used", "LINE", line.name());
            return false;
        }
        line.request({"", gpiod::line_request::DIRECTION_OUTPUT, 0},
                     gpioPolarities[i]);
        debug("Set GPIO line {LINE} to {POLARITY}", "LINE", line.name(),
              "POLARITY", gpioPolarities[i]);
    }

    return true;
}

std::string EEPROMDevice::getDriverPath()
{
    std::string model = chipModel;
    std::transform(model.begin(), model.end(), model.begin(), ::tolower);
    return "/sys/bus/i2c/drivers/" + model;
}

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
        debug("Found EEPROM device path: {PATH}", "PATH", devicePath);
        return devicePath;
    }

    return "";
}
// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> EEPROMDevice::bindEEPROM()
// NOLINTEND(readability-static-accessed-through-instance)
{
    info("Binding {DEVICE} EEPROM device", "DEVICE", getI2CDeviceId());
    std::string bindPath = getDriverPath() + "/bind";
    std::ofstream ofbind(bindPath, std::ofstream::out);
    if (!ofbind)
    {
        error("Failed to open bind file: {PATH}", "PATH", bindPath);
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
    debug("Unbinding EEPROM device {DEVICE}", "DEVICE", getI2CDeviceId());
    std::string unbindPath = getDriverPath() + "/unbind";
    std::ofstream ofunbind(unbindPath, std::ofstream::out);
    if (!ofunbind)
    {
        error("Failed to open unbind file: {PATH}", "PATH", unbindPath);
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
    std::string path = getDriverPath() + "/" + getI2CDeviceId();
    return std::filesystem::exists(path);
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> EEPROMDevice::writeEEPROM(const uint8_t* image,
                                                       size_t image_size)
// NOLINTEND(readability-static-accessed-through-instance)
{
    bool success = false;

    if (!gpioLines.empty())
    {
        debug("Requesting gpios to mux EEPROM device to BMC");
        if (!setGPIOs(gpioLines, gpioPolarities))
        {
            error("Failed to set GPIOs for EEPROM device");
            co_return success;
        }
    }

    this->setUpdateProgress(20);

    success = co_await writeEEPROMGPIOSet(image, image_size);
    if (success)
    {
        info("Successfully wrote EEPROM device");
        this->setUpdateProgress(90);
    }
    else
    {
        error("Failed to write EEPROM device");
    }

    if (!gpioLines.empty())
    {
        std::vector<uint8_t> invertedGpioPolarities;
        invertedGpioPolarities.reserve(gpioPolarities.size());
        std::transform(gpioPolarities.begin(), gpioPolarities.end(),
                       std::back_inserter(invertedGpioPolarities),
                       [](uint8_t value) { return value ^ 1; });

        debug("Requesting GPIOs to restore EEPROM device state.");
        if (!setGPIOs(gpioLines, invertedGpioPolarities))
        {
            error("Failed to restore EEPROM device state");
        }
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
        debug("Unbinding already bound EEPROM device");
        success = co_await unbindEEPROM();

        if (!success)
        {
            error("Error unbinding EEPROM device");
            co_return false;
        }
    }

    success = co_await bindEEPROM();
    if (!success)
    {
        error("Failed to bind EEPROM device");
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
        error("Failed to open EEPROM file: {PATH}", "PATH", eepromPath);
        co_return false;
    }

    debug("Writing EEPROM device, image size: {SIZE}", "SIZE", image_size);

    if (image_size >
        static_cast<size_t>(std::numeric_limits<std::streamsize>::max()))
    {
        error("Image size {SIZE} exceeds streamsize max limit", "SIZE",
              image_size);
        co_return false;
    }

    eepromFile.write(reinterpret_cast<const char*>(image),
                     static_cast<std::streamsize>(image_size));
    if (!eepromFile)
    {
        error("Failed to write data to EEPROM file");
        eepromFile.close();
        co_return false;
    }

    eepromFile.close();

    co_return true;
}

void EEPROMDevice::hostStateChangedUpdateVersion(sdbusplus::message_t& msg)
{
    if (versionProvider->isHostOnRequiredToGetVersion())
    {
        std::string intf;
        std::map<std::string, std::variant<std::string>> props;
        msg.read(intf, props);

        auto it = props.find("CurrentHostState");
        if (it != props.end())
        {
            auto& propVal = std::get<std::string>(it->second);
            // Ensure the version is not set when the host is off,
            // as the version can only be retrieved when the host is on.
            if (propVal.find("Off") == std::string::npos)
            {
                std::string version = versionProvider->getVersion();
                if (!version.empty())
                {
                    softwareCurrent->setVersion(version);
                    return;
                }
            }
        }
    }
}
