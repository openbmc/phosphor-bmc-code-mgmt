#include "eeprom_device.hpp"

#include "common/include/software.hpp"
#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/message.hpp>

#include <filesystem>
#include <fstream>

PHOSPHOR_LOG2_USING;

namespace fs = std::filesystem;
namespace MatchRules = sdbusplus::bus::match::rules;
namespace State = sdbusplus::common::xyz::openbmc_project::state;

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

static std::string getDriverPath(const std::string& chipModel)
{
    // Currently, only EEPROM chips with the model AT24 are supported.
    if (chipModel.find("EEPROM_24C") == std::string::npos)
    {
        error("Invalid EEPROM chip model: {CHIP}", "CHIP", chipModel);
        return "";
    }

    std::string path = "/sys/bus/i2c/drivers/at24";
    return std::filesystem::exists(path) ? path : "";
}

static std::string getI2CDeviceId(const uint16_t bus, const uint8_t address)
{
    std::ostringstream oss;
    oss << bus << "-" << std::hex << std::setfill('0') << std::setw(4)
        << static_cast<int>(address);
    return oss.str();
}

static std::string getEEPROMPath(const uint16_t bus, const uint8_t address)
{
    std::string devicePath =
        "/sys/bus/i2c/devices/" + getI2CDeviceId(bus, address) + "/eeprom";

    if (fs::exists(devicePath) && fs::is_regular_file(devicePath))
    {
        debug("Found EEPROM device path: {PATH}", "PATH", devicePath);
        return devicePath;
    }

    return "";
}

EEPROMDevice::EEPROMDevice(
    sdbusplus::async::context& ctx, const uint16_t bus, const uint8_t address,
    const std::string& chipModel, const std::vector<std::string>& gpioLines,
    const std::vector<bool>& gpioPolarities,
    std::unique_ptr<DeviceVersion> deviceVersion, SoftwareConfig& config,
    ManagerInf::SoftwareManager* parent) :
    Device(ctx, config, parent,
           {RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset}),
    bus(bus), address(address), chipModel(chipModel), gpioLines(gpioLines),
    gpioPolarities(gpioPolarities), deviceVersion(std::move(deviceVersion)),
    hostPower(ctx)
{
    // Some EEPROM devices require the host to be in a specific state before
    // retrieving the version. To handle this, set up a match to listen for
    // property changes on the host state. Once the host reaches the required
    // condition, the version can be updated accordingly.
    ctx.spawn(processHostStateChange());

    debug("Initialized EEPROM device instance on dbus");
}

sdbusplus::async::task<bool> EEPROMDevice::updateDevice(const uint8_t* image,
                                                        size_t image_size)
{
    std::vector<std::unique_ptr<::gpiod::line_bulk>> lineBulks;

    if (!gpioLines.empty())
    {
        debug("Requesting GPIOs to mux EEPROM to BMC");

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

    auto success = co_await writeEEPROM(image, image_size);

    if (success)
    {
        debug("Successfully wrote EEPROM");
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

        debug("Requesting GPIOs to mux EEPROM back to device");

        lineBulks = requestMuxGPIOs(gpioLines, gpioPolarities, true);

        if (lineBulks.empty())
        {
            error("Failed to mux EEPROM back to device");
            co_return false;
        }

        for (auto& lineBulk : lineBulks)
        {
            lineBulk->release();
        }
    }

    if (success)
    {
        debug("EEPROM device successfully updated");
        setUpdateProgress(100);
    }
    else
    {
        error("Failed to update EEPROM device");
    }

    co_return success;
}

sdbusplus::async::task<bool> EEPROMDevice::bindEEPROM()
{
    auto i2cDeviceId = getI2CDeviceId(bus, address);

    debug("Binding {I2CDEVICE} EEPROM", "I2CDEVICE", i2cDeviceId);

    if (isEEPROMBound())
    {
        debug("EEPROM was already bound, unbinding it now");
        if (!co_await unbindEEPROM())
        {
            error("Error unbinding EEPROM");
            co_return false;
        }
    }

    auto driverPath = getDriverPath(chipModel);
    if (driverPath.empty())
    {
        error("Driver path not found for chip model: {CHIP}", "CHIP",
              chipModel);
        co_return false;
    }

    auto bindPath = driverPath + "/bind";
    std::ofstream ofbind(bindPath, std::ofstream::out);
    if (!ofbind)
    {
        error("Failed to open bind file: {PATH}", "PATH", bindPath);
        co_return false;
    }

    ofbind << i2cDeviceId;
    ofbind.close();

    // wait for kernel
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(2));

    auto bound = isEEPROMBound();
    if (!bound)
    {
        error("Failed to bind {I2CDEVICE} EEPROM", "I2CDEVICE", i2cDeviceId);
    }

    co_return bound;
}
sdbusplus::async::task<bool> EEPROMDevice::unbindEEPROM()
{
    auto i2cDeviceId = getI2CDeviceId(bus, address);

    debug("Unbinding EEPROM device {I2CDEVICE}", "I2CDEVICE", i2cDeviceId);

    auto driverPath = getDriverPath(chipModel);
    if (driverPath.empty())
    {
        error("Failed to unbind EEPROM, driver path not found for chip {CHIP}",
              "CHIP", chipModel);
        co_return false;
    }

    auto unbindPath = driverPath + "/unbind";
    std::ofstream ofunbind(unbindPath, std::ofstream::out);
    if (!ofunbind)
    {
        error("Failed to open unbind file: {PATH}", "PATH", unbindPath);
        co_return false;
    }
    ofunbind << i2cDeviceId;
    ofunbind.close();

    // wait for kernel
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(2));

    auto bound = isEEPROMBound();
    if (bound)
    {
        error("Failed to unbind {I2CDEVICE} EEPROM", "I2CDEVICE", i2cDeviceId);
    }

    co_return !bound;
}

bool EEPROMDevice::isEEPROMBound()
{
    auto driverPath = getDriverPath(chipModel);

    if (driverPath.empty())
    {
        error("Failed to check if EEPROM is bound");
        return false;
    }

    auto i2cDeviceId = getI2CDeviceId(bus, address);

    return std::filesystem::exists(driverPath + "/" + i2cDeviceId);
}

sdbusplus::async::task<bool> EEPROMDevice::writeEEPROM(const uint8_t* image,
                                                       size_t image_size) const
{
    auto eepromPath = getEEPROMPath(bus, address);
    if (eepromPath.empty())
    {
        error("EEPROM file not found for device: {DEVICE}", "DEVICE",
              getI2CDeviceId(bus, address));
        co_return -1;
    }
    const std::string path =
        "/tmp/eeprom-image-" +
        std::to_string(SoftwareInf::Software::getRandomId()) + ".bin";

    int fd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0)
    {
        error("Failed to open file: {PATH}", "PATH", path);
        co_return -1;
    }

    const ssize_t bytesWritten = write(fd, image, image_size);

    close(fd);

    if (bytesWritten < 0 || static_cast<size_t>(bytesWritten) != image_size)
    {
        error("Failed to write image to file");
        co_return -1;
    }

    debug("Wrote {SIZE} bytes to {PATH}", "SIZE", bytesWritten, "PATH", path);

    std::string cmd = "dd if=" + path + " of=" + eepromPath + " bs=1k";

    debug("Running {CMD}", "CMD", cmd);

    auto success = co_await asyncSystem(ctx, cmd);

    std::filesystem::remove(path);

    co_return success;
}

sdbusplus::async::task<> EEPROMDevice::processHostStateChange()
{
    auto requiredHostState = deviceVersion->getHostStateToQueryVersion();

    if (!requiredHostState)
    {
        error("Failed to get required host state");
        co_return;
    }

    while (!ctx.stop_requested())
    {
        auto nextResult = co_await hostPower.stateChangedMatch.next<
            std::string, std::map<std::string, std::variant<std::string>>>();

        auto [interfaceName, changedProperties] = nextResult;

        auto it = changedProperties.find("CurrentHostState");
        if (it != changedProperties.end())
        {
            const auto& currentHostState = std::get<std::string>(it->second);

            if (currentHostState ==
                State::convertForMessage(*requiredHostState))
            {
                debug("Host state {STATE} matches to retrieve the version",
                      "STATE", currentHostState);
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
