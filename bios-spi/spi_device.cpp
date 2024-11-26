#include "spi_device.hpp"

#include "common/include/NotifyWatch.hpp"
#include "common/include/device.hpp"
#include "common/include/host_power.hpp"
#include "common/include/software_manager.hpp"

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

#include <fstream>
#include <random>

PHOSPHOR_LOG2_USING;

using namespace std::literals;
using namespace phosphor::software;
using namespace phosphor::software::manager;
using namespace phosphor::software::host_power;

SPIDevice::SPIDevice(sdbusplus::async::context& ctx,
                     uint64_t spiControllerIndex, uint64_t spiDeviceIndex,
                     bool dryRun, const std::vector<std::string>& gpioLinesIn,
                     const std::vector<uint64_t>& gpioValuesIn,
                     SoftwareConfig& config, SoftwareManager* parent,
                     enum FLASH_LAYOUT layout, enum FLASH_TOOL tool) :
    Device(ctx, config, parent,
           {RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset}),
    NotifyWatchIntf(ctx, biosVersionDirPath), dryRun(dryRun),
    gpioLines(gpioLinesIn),
    gpioValues(gpioValuesIn.begin(), gpioValuesIn.end()),
    spiControllerIndex(spiControllerIndex), spiDeviceIndex(spiDeviceIndex),
    layout(layout), tool(tool)
{
    std::map<uint32_t, std::string> spiDevAddr = {
        {0, "1e620000.spi"},
        {1, "1e630000.spi"},
        {2, "1e631000.spi"},
    };

    spiDev = spiDevAddr[spiControllerIndex];

    ctx.spawn(readNotifyAsync());

    debug("SPI Device '{NAME}' at {CI}:{DI} initialized successfully", "NAME",
          config.configName, "CI", spiControllerIndex, "DI", spiDeviceIndex);
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDevice::updateDevice(const uint8_t* image,
                                                     size_t image_size)
// NOLINTEND(readability-static-accessed-through-instance)
{
    // NOLINTBEGIN(readability-static-accessed-through-instance)
    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    auto prevPowerstate = co_await HostPower::getHostPowerstate(ctx);

    if (prevPowerstate == POWER_STATE_INVALID)
    {
        co_return false;
    }

    // NOLINTBEGIN(readability-static-accessed-through-instance)
    bool success = co_await HostPower::setHostPowerstate(ctx, POWER_STATE_OFF);
    // NOLINTEND(readability-static-accessed-through-instance)
    if (!success)
    {
        error("error changing host power state");
        co_return false;
    }
    setUpdateProgress(10);

    setUpdateProgress(20);

    success = co_await writeSPIFlash(image, image_size);

    if (success)
    {
        setUpdateProgress(100);
    }

    // restore the previous powerstate
    const bool powerstate_restore =
        co_await HostPower::setHostPowerstate(ctx, prevPowerstate);
    if (!powerstate_restore)
    {
        error("error changing host power state");
        co_return false;
    }

    // return value here is only describing if we successfully wrote to the
    // SPI flash. Restoring powerstate can still fail.
    co_return success;
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    // NOLINTEND(readability-static-accessed-through-instance)
}

const std::string spiAspeedSMCPath = "/sys/bus/platform/drivers/spi-aspeed-smc";

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDevice::bindSPIFlash()
// NOLINTEND(readability-static-accessed-through-instance)
{
    debug("binding flash to SMC");

    if (SPIDevice::isSPIFlashBound())
    {
        debug("flash was already bound, unbinding it now");
        bool success = co_await SPIDevice::unbindSPIFlash();

        if (!success)
        {
            error("error unbinding spi flash");
            co_return false;
        }
    }

    std::ofstream ofbind(spiAspeedSMCPath + "/bind", std::ofstream::out);
    ofbind << spiDev;
    ofbind.close();

    // wait for kernel
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(2));

    const bool isBound = isSPIFlashBound();

    if (!isBound)
    {
        error("failed to bind spi device");
    }

    co_return isBound;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDevice::unbindSPIFlash()
// NOLINTEND(readability-static-accessed-through-instance)
{
    debug("unbinding flash from SMC");
    std::ofstream ofunbind(spiAspeedSMCPath + "/unbind", std::ofstream::out);
    ofunbind << spiDev;
    ofunbind.close();

    // wait for kernel
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(2));

    co_return !isSPIFlashBound();
}

bool SPIDevice::isSPIFlashBound()
{
    std::string path = spiAspeedSMCPath + "/" + spiDev;

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

    std::vector<int> valuesInverted;
    valuesInverted.reserve(gpioValues.size());

    for (int value : gpioValues)
    {
        valuesInverted.push_back(value ? 0 : 1);
    }

    debug("[gpio] requesting gpios to mux SPI to BMC");

    auto bulk = std::make_unique<::gpiod::line_bulk>(lines);

    if (inverted)
    {
        bulk->request(config, valuesInverted);
    }
    else
    {
        bulk->request(config, gpioValues);
    }

    return bulk;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDevice::writeSPIFlash(const uint8_t* image,
                                                      size_t image_size)
// NOLINTEND(readability-static-accessed-through-instance)
{
    debug("[gpio] requesting gpios to mux SPI to BMC");

    std::unique_ptr<::gpiod::line_bulk> bulk =
        requestMuxGPIOs(gpioLines, gpioValues, false);

    if (!bulk)
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
            if (tool == FLASH_TOOL_FLASHROM)
            {
                success = co_await SPIDevice::writeSPIFlashWithFlashrom(
                    image, image_size);
            }
            else
            {
                success =
                    co_await SPIDevice::writeSPIFlashDefault(image, image_size);
            }
        }
    }

    success = success && co_await SPIDevice::unbindSPIFlash();

    bulk->release();

    // switch bios flash back to host via mux / GPIO
    // (not assume there is a pull to the default value)
    debug("[gpio] requesting gpios to mux SPI to Host");

    bulk = requestMuxGPIOs(gpioLines, gpioValues, true);

    if (!bulk)
    {
        co_return success;
    }

    bulk->release();

    co_return success;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDevice::writeSPIFlashWithFlashrom(
    const uint8_t* image, size_t image_size) const
// NOLINTEND(readability-static-accessed-through-instance)
{
    // randomize the name to enable parallel updates
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(1, 100000);
    const int r = distrib(gen);
    const std::string path =
        "/tmp/spi-device-image-" + std::to_string(r) + ".bin";

    int fd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0)
    {
        error("Failed to open file: {PATH}", "PATH", path);
        co_return false;
    }

    const ssize_t bytesWritten = write(fd, image, image_size);

    close(fd);

    if (bytesWritten < 0 || static_cast<size_t>(bytesWritten) != image_size)
    {
        error("Failed to write image to file");
        co_return false;
    }

    debug("wrote {SIZE} bytes to {PATH}", "SIZE", bytesWritten, "PATH", path);

    auto optDevPath = getMTDDevicePath();

    if (!optDevPath.has_value())
    {
        co_return false;
    }

    const auto& devPath = optDevPath.value();

    std::string cmd = "flashrom -p linux_mtd:dev=" + devPath;

    if (layout == FLASH_LAYOUT_FLAT)
    {
        cmd += "-w " + path;
    }
    else if (layout == FLASH_LAYOUT_INTEL_FLASH_DESCRIPTOR)
    {
        cmd += "--ifd -i fd -i bios -i me -w " + path;
    }
    else
    {
        error("unsupported flash layout");

        co_return false;
    }

    debug("[flashrom] running {CMD}", "CMD", cmd);

    const int exitCode = std::system(cmd.c_str());

    if (exitCode != 0)
    {
        error("error running flaashrom");
    }

    std::filesystem::remove(path);

    co_return exitCode;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDevice::writeSPIFlashDefault(
    const uint8_t* image, size_t image_size)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto optDevPath = getMTDDevicePath();

    if (!optDevPath.has_value())
    {
        co_return false;
    }

    const auto& devPath = optDevPath.value();

    int fd = open(devPath.c_str(), O_WRONLY);
    if (fd < 0)
    {
        error("Failed to open device: {PATH}", "PATH", devPath);
        co_return false;
    }

    ssize_t bytesWritten = write(fd, image, image_size);

    close(fd);

    if (bytesWritten < 0)
    {
        error("Failed to write to device");
        co_return false;
    }

    if (static_cast<size_t>(bytesWritten) != image_size)
    {
        error("Incomplete write to device");
        co_return false;
    }

    debug("Successfully wrote {NBYTES} bytes to {PATH}", "NBYTES", bytesWritten,
          "PATH", devPath);

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

// NOLINTNEXTLINE(readability-static-accessed-through-instance)
auto SPIDevice::processVersionUpdate(std::string configFileName)
    -> sdbusplus::async::task<>
{
    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch)
    if (biosVersionFilename != configFileName)
    {
        error(
            "Update config file name '{NAME}' (!= '{EXPECTED}') is not expected",
            "NAME", configFileName, "EXPECTED", biosVersionFilename);
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

    error("Error: No MTD device found for spi {CI}.{DI}", "CI",
          spiControllerIndex, "DI", spiDeviceIndex);

    return std::nullopt;
}
