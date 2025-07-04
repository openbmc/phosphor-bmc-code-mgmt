#include "spi_device.hpp"

#include "common/include/NotifyWatch.hpp"
#include "common/include/device.hpp"
#include "common/include/host_power.hpp"
#include "common/include/software_manager.hpp"
#include "common/include/utils.hpp"

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

#include <cstddef>
#include <fstream>
#include <random>

PHOSPHOR_LOG2_USING;

using namespace std::literals;
using namespace phosphor::software;
using namespace phosphor::software::manager;
using namespace phosphor::software::host_power;

static std::optional<std::string> getSPIDevAddr(uint64_t spiControllerIndex)
{
    const fs::path spi_path =
        "/sys/class/spi_master/spi" + std::to_string(spiControllerIndex);

    if (!fs::exists(spi_path))
    {
        error("SPI controller index not found at {PATH}", "PATH",
              spi_path.string());
        return std::nullopt;
    }

    fs::path target = fs::read_symlink(spi_path);

    // The path looks like
    // ../../devices/platform/ahb/1e630000.spi/spi_master/spi1 We want to
    // extract e.g. '1e630000.spi'

    for (const auto& part : target)
    {
        std::string part_str = part.string();
        if (part_str.find(".spi") != std::string::npos)
        {
            debug("Found SPI Address {ADDR}", "ADDR", part_str);
            return part_str;
        }
    }

    return std::nullopt;
}

SPIDevice::SPIDevice(sdbusplus::async::context& ctx,
                     uint64_t spiControllerIndex, uint64_t spiDeviceIndex,
                     bool dryRun, const std::vector<std::string>& gpioLinesIn,
                     const std::vector<uint64_t>& gpioValuesIn,
                     SoftwareConfig& config, SoftwareManager* parent,
                     enum FlashLayout layout, enum FlashTool tool,
                     const std::string& versionDirPath) :
    Device(ctx, config, parent,
           {RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset}),
    NotifyWatchIntf(ctx, versionDirPath), dryRun(dryRun),
    gpioLines(gpioLinesIn),
    gpioValues(gpioValuesIn.begin(), gpioValuesIn.end()),
    spiControllerIndex(spiControllerIndex), spiDeviceIndex(spiDeviceIndex),
    layout(layout), tool(tool)
{
    auto optAddr = getSPIDevAddr(spiControllerIndex);

    if (!optAddr.has_value())
    {
        throw std::invalid_argument("SPI controller index not found");
    }

    spiDev = optAddr.value();

    ctx.spawn(readNotifyAsync());

    debug(
        "SPI Device {NAME} at {CONTROLLERINDEX}:{DEVICEINDEX} initialized successfully",
        "NAME", config.configName, "CONTROLLERINDEX", spiControllerIndex,
        "DEVICEINDEX", spiDeviceIndex);
}

sdbusplus::async::task<bool> SPIDevice::updateDevice(const uint8_t* image,
                                                     size_t image_size)
{
    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch)
    auto prevPowerstate = co_await HostPower::getState(ctx);

    if (prevPowerstate != stateOn && prevPowerstate != stateOff)
    {
        co_return false;
    }

    bool success = co_await HostPower::setState(ctx, stateOff);
    if (!success)
    {
        error("error changing host power state");
        co_return false;
    }
    setUpdateProgress(10);

    success = co_await writeSPIFlash(image, image_size);

    if (success)
    {
        setUpdateProgress(100);
    }

    // restore the previous powerstate
    const bool powerstate_restore =
        co_await HostPower::setState(ctx, prevPowerstate);
    if (!powerstate_restore)
    {
        error("error changing host power state");
        co_return false;
    }

    // return value here is only describing if we successfully wrote to the
    // SPI flash. Restoring powerstate can still fail.
    co_return success;
}

const std::string spiAspeedSMCPath = "/sys/bus/platform/drivers/spi-aspeed-smc";
const std::string spiNorPath = "/sys/bus/spi/drivers/spi-nor";

sdbusplus::async::task<bool> SPIDevice::bindSPIFlash()
{
    if (!SPIDevice::isSPIControllerBound())
    {
        debug("binding flash to SMC");
        std::ofstream ofbind(spiAspeedSMCPath + "/bind", std::ofstream::out);
        ofbind << spiDev;
        ofbind.close();
    }

    const int driverBindSleepDuration = 2;

    co_await sdbusplus::async::sleep_for(
        ctx, std::chrono::seconds(driverBindSleepDuration));

    if (!isSPIControllerBound())
    {
        error("failed to bind spi controller");
        co_return false;
    }

    const std::string name =
        std::format("spi{}.{}", spiControllerIndex, spiDeviceIndex);

    std::ofstream ofbindSPINor(spiNorPath + "/bind", std::ofstream::out);
    ofbindSPINor << name;
    ofbindSPINor.close();

    co_await sdbusplus::async::sleep_for(
        ctx, std::chrono::seconds(driverBindSleepDuration));

    if (!isSPIFlashBound())
    {
        error("failed to bind spi flash (spi-nor driver)");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> SPIDevice::unbindSPIFlash()
{
    debug("unbinding flash");

    const std::string name =
        std::format("spi{}.{}", spiControllerIndex, spiDeviceIndex);

    std::ofstream ofunbind(spiNorPath + "/unbind", std::ofstream::out);
    ofunbind << name;
    ofunbind.close();

    // wait for kernel
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(2));

    co_return !isSPIFlashBound();
}

bool SPIDevice::isSPIControllerBound()
{
    std::string path = spiAspeedSMCPath + "/" + spiDev;

    return std::filesystem::exists(path);
}

bool SPIDevice::isSPIFlashBound()
{
    const std::string name =
        std::format("spi{}.{}", spiControllerIndex, spiDeviceIndex);
    std::string path = spiNorPath + "/" + name;

    return std::filesystem::exists(path);
}

static std::unique_ptr<::gpiod::line_bulk> requestMuxGPIOs(
    const std::vector<std::string>& gpioLines,
    const std::vector<int>& gpioValues, bool inverted)
{
    std::vector<::gpiod::line> lines;

    for (const std::string& lineName : gpioLines)
    {
        const ::gpiod::line line = ::gpiod::find_line(lineName);

        if (line.is_used())
        {
            error("gpio line {LINE} was still used", "LINE", lineName);
            return nullptr;
        }

        lines.push_back(line);
    }

    ::gpiod::line_request config{"", ::gpiod::line_request::DIRECTION_OUTPUT,
                                 0};

    debug("requesting gpios for mux");

    auto lineBulk = std::make_unique<::gpiod::line_bulk>(lines);

    if (inverted)
    {
        std::vector<int> valuesInverted;
        valuesInverted.reserve(gpioValues.size());

        for (int value : gpioValues)
        {
            valuesInverted.push_back(value ? 0 : 1);
        }

        lineBulk->request(config, valuesInverted);
    }
    else
    {
        lineBulk->request(config, gpioValues);
    }

    return lineBulk;
}

sdbusplus::async::task<bool> SPIDevice::writeSPIFlash(const uint8_t* image,
                                                      size_t image_size)
{
    debug("[gpio] requesting gpios to mux SPI to BMC");

    std::unique_ptr<::gpiod::line_bulk> lineBulk =
        requestMuxGPIOs(gpioLines, gpioValues, false);

    if (!lineBulk)
    {
        co_return false;
    }

    bool success = co_await SPIDevice::bindSPIFlash();
    if (success)
    {
        if (dryRun)
        {
            info("dry run, NOT writing to the chip");
        }
        else
        {
            if (tool == flashToolFlashrom)
            {
                success = co_await SPIDevice::writeSPIFlashWithFlashrom(
                    image, image_size);
                if (!success)
                {
                    error(
                        "Error writing to SPI flash {CONTROLLERINDEX}:{DEVICEINDEX}",
                        "CONTROLLERINDEX", spiControllerIndex, "DEVICEINDEX",
                        spiDeviceIndex);
                }
            }
            else if (tool == flashToolFlashcp)
            {
                success = co_await SPIDevice::writeSPIFlashWithFlashcp(
                    image, image_size);
            }
            else
            {
                success =
                    co_await SPIDevice::writeSPIFlashDefault(image, image_size);
            }
        }

        success = success && co_await SPIDevice::unbindSPIFlash();
    }

    lineBulk->release();

    // switch bios flash back to host via mux / GPIO
    // (not assume there is a pull to the default value)
    debug("[gpio] requesting gpios to mux SPI to Host");

    lineBulk = requestMuxGPIOs(gpioLines, gpioValues, true);

    if (!lineBulk)
    {
        co_return success;
    }

    lineBulk->release();

    co_return success;
}

sdbusplus::async::task<bool> SPIDevice::writeSPIFlashWithFlashrom(
    const uint8_t* image, size_t image_size) const
{
    // randomize the name to enable parallel updates
    const std::string path = "/tmp/spi-device-image-" +
                             std::to_string(Software::getRandomId()) + ".bin";

    int fd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0)
    {
        error("Failed to open file: {PATH}", "PATH", path);
        co_return 1;
    }

    const ssize_t bytesWritten = write(fd, image, image_size);

    close(fd);

    setUpdateProgress(30);

    if (bytesWritten < 0 || static_cast<size_t>(bytesWritten) != image_size)
    {
        error("Failed to write image to file");
        co_return 1;
    }

    debug("wrote {SIZE} bytes to {PATH}", "SIZE", bytesWritten, "PATH", path);

    auto devPath = getMTDDevicePath();

    if (!devPath.has_value())
    {
        co_return 1;
    }

    size_t devNum = 0;

    try
    {
        devNum = std::stoi(devPath.value().substr(8));
    }
    catch (std::exception& e)
    {
        error("could not parse mtd device number from {STR}: {ERROR}", "STR",
              devPath.value(), "ERROR", e);
        co_return 1;
    }

    std::string cmd = "flashrom -p linux_mtd:dev=" + std::to_string(devNum);

    if (layout == flashLayoutFlat)
    {
        cmd += " -w " + path;
    }
    else
    {
        error("unsupported flash layout");

        co_return 1;
    }

    debug("[flashrom] running {CMD}", "CMD", cmd);

    auto success = co_await asyncSystem(ctx, cmd);

    std::filesystem::remove(path);

    co_return success;
}

sdbusplus::async::task<bool> SPIDevice::writeSPIFlashWithFlashcp(
    const uint8_t* image, size_t image_size) const
{
    // randomize the name to enable parallel updates
    const std::string path = "/tmp/spi-device-image-" +
                             std::to_string(Software::getRandomId()) + ".bin";

    int fd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0)
    {
        error("Failed to open file: {PATH}", "PATH", path);
        co_return 1;
    }

    const ssize_t bytesWritten = write(fd, image, image_size);

    close(fd);

    setUpdateProgress(30);

    if (bytesWritten < 0 || static_cast<size_t>(bytesWritten) != image_size)
    {
        error("Failed to write image to file");
        co_return 1;
    }

    debug("wrote {SIZE} bytes to {PATH}", "SIZE", bytesWritten, "PATH", path);

    auto devPath = getMTDDevicePath();

    if (!devPath.has_value())
    {
        co_return 1;
    }

    std::string cmd = std::format("flashcp -v {} {}", path, devPath.value());

    debug("running {CMD}", "CMD", cmd);

    auto success = co_await asyncSystem(ctx, cmd);

    std::filesystem::remove(path);

    co_return success;
}

sdbusplus::async::task<bool> SPIDevice::writeSPIFlashDefault(
    const uint8_t* image, size_t image_size)
{
    auto devPath = getMTDDevicePath();

    if (!devPath.has_value())
    {
        co_return false;
    }

    int fd = open(devPath.value().c_str(), O_WRONLY);
    if (fd < 0)
    {
        error("Failed to open device: {PATH}", "PATH", devPath.value());
        co_return false;
    }

    // Write the image in chunks to avoid blocking for too long.
    // Also, to provide meaningful progress updates.

    const size_t chunk = static_cast<size_t>(1024 * 1024);
    ssize_t bytesWritten = 0;

    const int progressStart = 30;
    const int progressEnd = 90;

    for (size_t offset = 0; offset < image_size; offset += chunk)
    {
        const ssize_t written =
            write(fd, image + offset, std::min(chunk, image_size - offset));

        if (written < 0)
        {
            error("Failed to write to device");
            co_return false;
        }

        bytesWritten += written;

        setUpdateProgress(
            progressStart + int((progressEnd - progressStart) *
                                (double(offset) / double(image_size))));
    }

    close(fd);

    if (static_cast<size_t>(bytesWritten) != image_size)
    {
        error("Incomplete write to device");
        co_return false;
    }

    debug("Successfully wrote {NBYTES} bytes to {PATH}", "NBYTES", bytesWritten,
          "PATH", devPath.value());

    co_return true;
}

std::string SPIDevice::getVersion()
{
    std::string version{};
    try
    {
        std::ifstream config(biosVersionPath);

        config >> version;
    }
    catch (std::exception& e)
    {
        error("Failed to get version with {ERROR}", "ERROR", e.what());
        version = versionUnknown;
    }

    if (version.empty())
    {
        version = versionUnknown;
    }

    return version;
}

auto SPIDevice::processUpdate(std::string versionFileName)
    -> sdbusplus::async::task<>
{
    if (biosVersionFilename != versionFileName)
    {
        error(
            "Update config file name '{NAME}' (!= '{EXPECTED}') is not expected",
            "NAME", versionFileName, "EXPECTED", biosVersionFilename);
        co_return;
    }

    if (softwareCurrent)
    {
        softwareCurrent->setVersion(getVersion());
    }

    co_return;
}

std::optional<std::string> SPIDevice::getMTDDevicePath() const
{
    const std::string spiPath =
        "/sys/class/spi_master/spi" + std::to_string(spiControllerIndex) +
        "/spi" + std::to_string(spiControllerIndex) + "." +
        std::to_string(spiDeviceIndex) + "/mtd/";

    if (!std::filesystem::exists(spiPath))
    {
        error("Error: SPI path not found: {PATH}", "PATH", spiPath);
        return "";
    }

    for (const auto& entry : std::filesystem::directory_iterator(spiPath))
    {
        const std::string mtdName = entry.path().filename().string();

        if (mtdName.starts_with("mtd") && !mtdName.ends_with("ro"))
        {
            return "/dev/" + mtdName;
        }
    }

    error("Error: No MTD device found for spi {CONTROLLERINDEX}.{DEVICEINDEX}",
          "CONTROLLERINDEX", spiControllerIndex, "DEVICEINDEX", spiDeviceIndex);

    return std::nullopt;
}
