#include "eeprom_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/message.hpp>

#include <filesystem>
#include <fstream>

PHOSPHOR_LOG2_USING;

namespace fs = std::filesystem;
namespace MatchRules = sdbusplus::bus::match::rules;
using sdbusplus::common::xyz::openbmc_project::state::convertForMessage;

static std::vector<std::unique_ptr<::gpiod::line_bulk>> requestMuxGPIOs(
    const std::vector<std::string>& gpioLines,
    const std::vector<bool>& gpioPolarities, bool inverted)
{
    std::map<std::string, std::vector<std::string>> groupLineNames;
    std::map<std::string, std::vector<int>> groupValues;

    for (size_t i = 0; i < gpioLines.size(); ++i)
    {
        auto line = ::gpiod::find_line(gpioLines[i]);

        if (!line)
        {
            error("Failed to find GPIO line: {LINE}", "LINE", gpioLines[i]);
            return {};
        }

        if (line.is_used())
        {
            error("GPIO line {LINE} was still used", "LINE", gpioLines[i]);
            return {};
        }

        std::string chipName = line.get_chip().name();
        groupLineNames[chipName].push_back(gpioLines[i]);
        groupValues[chipName].push_back(gpioPolarities[i] ^ inverted ? 1 : 0);
    }

    std::vector<std::unique_ptr<::gpiod::line_bulk>> lineBulks;
    ::gpiod::line_request config{"", ::gpiod::line_request::DIRECTION_OUTPUT,
                                 0};

    for (auto& [chipName, lineNames] : groupLineNames)
    {
        ::gpiod::chip chip(chipName);
        std::vector<::gpiod::line> lines;

        for (size_t i = 0; i < lineNames.size(); ++i)
        {
            const auto& name = lineNames[i];
            auto line = chip.find_line(name);

            if (!line)
            {
                error("Failed to get {LINE} from chip {CHIP}", "LINE", name,
                      "CHIP", chipName);
                return {};
            }

            debug("Requesting chip {CHIP}, GPIO line {LINE} to {VALUE}", "CHIP",
                  chip.name(), "LINE", line.name(), "VALUE",
                  groupValues[chipName][i]);

            lines.push_back(std::move(line));
        }

        auto lineBulk = std::make_unique<::gpiod::line_bulk>(lines);

        if (!lineBulk)
        {
            error("Failed to create line bulk for chip={CHIP}", "CHIP",
                  chipName);
            return {};
        }

        lineBulk->request(config, groupValues[chipName]);

        lineBulks.push_back(std::move(lineBulk));
    }

    return lineBulks;
}

EEPROMDevice::EEPROMDevice(
    sdbusplus::async::context& ctx, const uint16_t& bus, const uint8_t& address,
    const std::string& chipModel, const std::vector<std::string>& gpioLines,
    const std::vector<bool>& gpioPolarities,
    std::unique_ptr<DeviceVersion> deviceVersion, SoftwareConfig& config,
    SoftwareManager* parent) :
    Device(ctx, config, parent,
           {RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset}),
    bus(bus), address(address), chipModel(chipModel), gpioLines(gpioLines),
    gpioPolarities(gpioPolarities), deviceVersion(std::move(deviceVersion))
{
    // Some EEPROM devices require the host to be in a specific state before
    // retrieving the version. To handle this, set up a match to listen for
    // property changes on the host state. Once the host reaches the required
    // condition, the version can be updated accordingly.
    hostPower = std::make_unique<HostPower>(ctx);
    ctx.spawn(hostStateChangedUpdateVersion());

    debug("Initialized EEPROM device instance on dbus");
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> EEPROMDevice::updateDevice(const uint8_t* image,
                                                        size_t image_size)
// NOLINTEND(readability-static-accessed-through-instance)
{
    // NOLINTBEGIN(readability-static-accessed-through-instance)
    bool success = co_await writeEEPROM(image, image_size);
    // NOLINTEND(readability-static-accessed-through-instance)
    if (!success)
    {
        error("Failed to update EEPROM device");
        co_return false;
    }

    info("EERPOM device successfully updated");

    co_return true;
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
    info("Binding {DEVICE} EEPROM", "DEVICE", getI2CDeviceId());

    if (isEEPROMBound())
    {
        debug("EEPROM was already bound, unbinding it now");
        if (!co_await unbindEEPROM())
        {
            error("Error unbinding EEPROM");
            co_return false;
        }
    }

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

    auto bound = isEEPROMBound();
    if (!bound)
    {
        error("Failed to bind {DEVICE} EEPROM", "DEVICE", getI2CDeviceId());
    }

    co_return bound;
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

    auto bound = isEEPROMBound();
    if (bound)
    {
        error("Failed to unbind {DEVICE} EEPROM", "DEVICE", getI2CDeviceId());
    }

    co_return !bound;
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
    std::vector<std::unique_ptr<::gpiod::line_bulk>> lineBulks;

    if (!gpioLines.empty())
    {
        debug("Requesting gpios to mux EEPROM to BMC");

        lineBulks = requestMuxGPIOs(gpioLines, gpioPolarities, false);

        if (lineBulks.empty())
        {
            error("Failed to mux EEPROM to BMC");
            co_return false;
        }
    }

    setUpdateProgress(20);

    if (!co_await bindEEPROM())
    {
        co_return false;
    }

    setUpdateProgress(40);

    bool success = co_await writeEEPROMGPIOSetDeviceBound(image, image_size);

    if (success)
    {
        info("Successfully wrote EEPROM");
        setUpdateProgress(60);
    }
    else
    {
        error("Failed to write EEPROM");
    }

    success = success && co_await unbindEEPROM();

    if (success)
    {
        setUpdateProgress(80);
    }

    if (!gpioLines.empty())
    {
        for (auto& lineBulk : lineBulks)
        {
            lineBulk->release();
        }

        debug("Requesting gpios to mux EEPROM to EEPROM device");

        lineBulks = requestMuxGPIOs(gpioLines, gpioPolarities, true);

        if (lineBulks.empty())
        {
            error("Failed to mux EEPROM to EEPROM device");
            co_return false;
        }

        for (auto& lineBulk : lineBulks)
        {
            lineBulk->release();
        }
    }

    if (success)
    {
        setUpdateProgress(100);
    }

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

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<> EEPROMDevice::hostStateChangedUpdateVersion()
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto requiredHostState = deviceVersion->getRequiredHostStateToGetVersion();

    if (!hostPower || !requiredHostState)
    {
        co_return;
    }

    while (!ctx.stop_requested())
    {
        auto [interfaceName, changedProperties] =
            co_await hostPower->stateChangedMatch
                .next<std::string,
                      std::map<std::string, std::variant<std::string>>>();

        auto it = changedProperties.find("CurrentHostState");
        if (it != changedProperties.end())
        {
            auto& propVal = std::get<std::string>(it->second);

            if (propVal == convertForMessage(*requiredHostState))
            {
                debug("Host state {STATE} matches to retrieve the version",
                      "STATE", propVal);
                std::string version = deviceVersion->getVersion();
                if (!version.empty())
                {
                    softwareCurrent->setVersion(version);
                }
            }
        }
    }

    co_return;
}
