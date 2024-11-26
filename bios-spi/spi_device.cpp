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

PHOSPHOR_LOG2_USING;

using namespace std::literals;
using namespace phosphor::software;
using namespace phosphor::software::manager;
using namespace phosphor::software::host_power;

SPIDevice::SPIDevice(sdbusplus::async::context& ctx,
                     uint64_t spiControllerIndex, uint64_t spiDeviceIndex,
                     bool dryRun, const std::vector<std::string>& gpioLines,
                     const std::vector<uint8_t>& gpioValues,
                     SoftwareConfig& config, SoftwareManager* parent,
                     bool layoutFlat, bool toolFlashrom) :
    Device(ctx, config, parent,
           {RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset}),
    NotifyWatchIntf(ctx, biosVersionDirPath), dryRun(dryRun),
    gpioLines(gpioLines), gpioValues(gpioValues),
    spiControllerIndex(spiControllerIndex), spiDeviceIndex(spiDeviceIndex),
    layoutFlat(layoutFlat), toolFlashrom(toolFlashrom)
{
    ctx.spawn(readNotifyAsync());

    debug("SPI Device '{NAME}' initialized successfully", "NAME",
          config.configName);
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
const std::string spiDev = "1e630000.spi";

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

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDevice::writeSPIFlash(const uint8_t* image,
                                                      size_t image_size)
// NOLINTEND(readability-static-accessed-through-instance)
{
    gpiod::chip chip;
    try
    {
        // TODO: make it work for multiple chips
        chip = gpiod::chip("/dev/gpiochip0");
    }
    catch (std::exception& e)
    {
        error(e.what());
        co_return false;
    }

    std::vector<unsigned int> offsets;

    for (const std::string& lineName : gpioLines)
    {
        const ::gpiod::line line = ::gpiod::find_line(lineName);

        if (line.is_used())
        {
            error("gpio line {LINE} was still used", "LINE", lineName);
            co_return false;
        }
        offsets.push_back(line.offset());
    }

    auto lines = chip.get_lines(offsets);

    ::gpiod::line_request config{"", ::gpiod::line_request::DIRECTION_OUTPUT,
                                 0};
    std::vector<int> values;
    std::vector<int> valuesInverted;
    values.reserve(gpioValues.size());

    for (uint8_t value : gpioValues)
    {
        values.push_back(value);
        valuesInverted.push_back(value ? 0 : 1);
    }

    debug("[gpio] requesting gpios to mux SPI to BMC");
    lines.request(config, values);

    bool success = co_await SPIDevice::bindSPIFlash();
    if (success)
    {
        if (dryRun)
        {
            info("dry run, NOT writing to the chip");
        }
        else
        {
            if (toolFlashrom)
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

    lines.release();

    // switch bios flash back to host via mux / GPIO
    // (not assume there is a pull to the default value)
    debug("[gpio] requesting gpios to mux SPI to Host");
    lines.request(config, valuesInverted);

    lines.release();

    co_return success;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDevice::writeSPIFlashWithFlashrom(
    const uint8_t* image, size_t image_size) const
// NOLINTEND(readability-static-accessed-through-instance)
{
    // TODO: randomize the name to enable parallel updates
    std::string path = "/tmp/spi-device-image.bin";
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

    std::string cmd = "flashrom -p linux_mtd:dev=/dev/mtd/by-name/pnor";

    if (layoutFlat)
    {
        cmd += "-w " + path;
    }
    else
    {
        cmd += "--ifd -i fd -i bios -i me -w " + path;
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
    const std::string& devPath = "/dev/mtd/by-name/pnor";
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
