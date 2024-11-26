#include "spi_device.hpp"

#include "common/include/device.hpp"
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

SPIDevice::SPIDevice(
    sdbusplus::async::context& ctx, const std::string& spiDevName, bool dryRun,
    bool hasME, const std::vector<std::string>& gpioLines,
    const std::vector<uint8_t>& gpioValues, SoftwareConfig& config,
    SoftwareManager* parent, bool layoutFlat, bool toolFlashrom) :
    Device(ctx, config, parent,
           {RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset}),
    dryRun(dryRun), hasManagementEngine(hasME), gpioLines(gpioLines),
    gpioValues(gpioValues), spiDev(spiDevName), layoutFlat(layoutFlat),
    toolFlashrom(toolFlashrom)
{
    debug("initialized SPI Device instance on dbus");
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDevice::updateDevice(
    const uint8_t* image, size_t image_size,
    std::unique_ptr<SoftwareActivationProgress>& activationProgress)
// NOLINTEND(readability-static-accessed-through-instance)
{
    // NOLINTBEGIN(readability-static-accessed-through-instance)
    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    bool success =
        co_await writeSPIFlash(image, image_size, activationProgress);
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    // NOLINTEND(readability-static-accessed-through-instance)
    co_return success;
}

constexpr const char* IPMB_SERVICE = "xyz.openbmc_project.Ipmi.Channel.Ipmb";
constexpr const char* IPMB_PATH = "/xyz/openbmc_project/Ipmi/Channel/Ipmb";
constexpr const char* IPMB_INTF = "org.openbmc.Ipmb";

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<> SPIDevice::setManagementEngineRecoveryMode()
// NOLINTEND(readability-static-accessed-through-instance)
{
    info("[ME] setting Management Engine to recovery mode");
    auto m = ctx.get_bus().new_method_call(IPMB_SERVICE, IPMB_PATH, IPMB_INTF,
                                           "sendRequest");

    // me address, 0x2e oen, 0x00 - lun, 0xdf - force recovery
    uint8_t cmd_recover[] = {0x1, 0x2e, 0x0, 0xdf};
    for (unsigned int i = 0; i < sizeof(cmd_recover); i++)
    {
        m.append(cmd_recover[i]);
    }
    std::vector<uint8_t> remainder = {0x04, 0x57, 0x01, 0x00, 0x01};
    m.append(remainder);

    m.call();

    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(5));

    co_return;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<> SPIDevice::resetManagementEngine()
// NOLINTEND(readability-static-accessed-through-instance)
{
    info("[ME] resetting Management Engine");
    auto m = ctx.get_bus().new_method_call(IPMB_SERVICE, IPMB_PATH, IPMB_INTF,
                                           "sendRequest");

    // me address, 0x6 App Fn, 0x00 - lun, 0x2 - cold reset
    uint8_t cmd_recover[] = {0x1, 0x6, 0x0, 0x2};
    for (unsigned int i = 0; i < sizeof(cmd_recover); i++)
    {
        m.append(cmd_recover[i]);
    }
    std::vector<uint8_t> remainder;
    m.append(remainder);

    m.call();

    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(5));

    co_return;
}

const std::string spiAspeedSMCPath = "/sys/bus/platform/drivers/spi-aspeed-smc";

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDevice::bindSPIFlash()
// NOLINTEND(readability-static-accessed-through-instance)
{
    info("[SPI] binding flash to SMC");
    std::ofstream ofbind(spiAspeedSMCPath + "/bind", std::ofstream::out);
    ofbind << spiDev;
    ofbind.close();

    // wait for kernel
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(2));

    co_return isSPIFlashBound();
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDevice::unbindSPIFlash()
// NOLINTEND(readability-static-accessed-through-instance)
{
    info("[SPI] unbinding flash from SMC");
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
    debug("[SPI] checking {PATH}", "PATH", path);

    return std::filesystem::exists(path);
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDevice::writeSPIFlash(
    const uint8_t* image, size_t image_size,
    std::unique_ptr<SoftwareActivationProgress>& progress)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::optional<bool> currentPowerstateOpt = std::nullopt;

    currentPowerstateOpt = co_await getHostPowerstate(ctx);

    if (!currentPowerstateOpt.has_value())
    {
        co_return false;
    }

    const bool prevPowerstate = currentPowerstateOpt.value();

    // NOLINTBEGIN(readability-static-accessed-through-instance)
    bool success = co_await setHostPowerstate(ctx, false);
    // NOLINTEND(readability-static-accessed-through-instance)
    if (!success)
    {
        error("error changing host power state");
        co_return false;
    }
    progress->setUpdateProgress(10);

    if (hasManagementEngine)
    {
        co_await setManagementEngineRecoveryMode();
    }
    progress->setUpdateProgress(20);

    success = co_await writeSPIFlashHostOff(image, image_size);

    if (success)
    {
        progress->setUpdateProgress(70);
    }

    if (hasManagementEngine)
    {
        co_await resetManagementEngine();
    }

    if (success)
    {
        progress->setUpdateProgress(100);
    }

    // restore the previous powerstate
    const bool powerstate_restore =
        co_await setHostPowerstate(ctx, prevPowerstate);
    if (!powerstate_restore)
    {
        error("error changing host power state");
        co_return false;
    }

    // return value here is only describing if we successfully wrote to the
    // SPI flash. Restoring powerstate can still fail.
    co_return success;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDevice::writeSPIFlashHostOff(
    const uint8_t* image, size_t image_size)
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

    co_await writeSPIFlashHostOffGPIOSet(image, image_size);

    lines.release();

    // switch bios flash back to host via mux / GPIO
    // (not assume there is a pull to the default value)
    debug("[gpio] requesting gpios to mux SPI to Host");
    lines.request(config, valuesInverted);

    lines.release();

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDevice::writeSPIFlashHostOffGPIOSet(
    const uint8_t* image, size_t image_size)
// NOLINTEND(readability-static-accessed-through-instance)
{
    bool success = true;

    if (SPIDevice::isSPIFlashBound())
    {
        debug("[SPI] flash was already bound, unbinding it now");
        success = co_await SPIDevice::unbindSPIFlash();

        if (!success)
        {
            error("[SPI] error unbinding spi flash");
            co_return false;
        }
    }

    success = co_await SPIDevice::bindSPIFlash();

    if (!success)
    {
        error("[SPI] failed to bind spi device");
        co_await SPIDevice::unbindSPIFlash();
        co_return false;
    }

    if (dryRun)
    {
        info("[SPI] dry run, NOT writing to the chip");
    }
    else
    {
        if (toolFlashrom)
        {
            co_await SPIDevice::writeSPIFlashFlashromHostOffGPIOSetDeviceBound(
                image, image_size);
        }
        else
        {
            co_await SPIDevice::writeSPIFlashHostOffGPIOSetDeviceBound(
                image, image_size);
        }
    }

    success = co_await SPIDevice::unbindSPIFlash();

    co_return success;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool>
    SPIDevice::writeSPIFlashFlashromHostOffGPIOSetDeviceBound(
        const uint8_t* image, size_t image_size) const
// NOLINTEND(readability-static-accessed-through-instance)
{
    // TODO: randomize the name to enable parallel updates
    std::string path = "/tmp/spi-device-image.bin";
    int fd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0)
    {
        error("[SPI] Failed to open file: {PATH}", "PATH", path);
        co_return false;
    }

    const ssize_t bytesWritten = write(fd, image, image_size);

    close(fd);

    if (bytesWritten < 0 || static_cast<size_t>(bytesWritten) != image_size)
    {
        error("[SPI] Failed to write image to file");
        co_return false;
    }

    debug("[SPI] wrote {SIZE} bytes to {PATH}", "SIZE", bytesWritten, "PATH",
          path);

    // TODO: do not hardcode the mtd device
    std::string cmd = "flashrom -p linux_mtd:dev=6 ";

    if (layoutFlat)
    {
        cmd += "-w " + path;
    }
    else
    {
        cmd += "--ifd -i fd -i bios -i me -w " + path;
    }

    info("[flashrom] running {CMD}", "CMD", cmd);

    const int exitCode = std::system(cmd.c_str());

    if (exitCode != 0)
    {
        error("[SPI] error running flaashrom");
    }

    std::filesystem::remove(path);

    co_return exitCode;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDevice::writeSPIFlashHostOffGPIOSetDeviceBound(
    const uint8_t* image, size_t image_size)
// NOLINTEND(readability-static-accessed-through-instance)
{
    // TODO: not hardcode the mtd device
    std::string devPath = "/dev/mtd6";
    int fd = open(devPath.c_str(), O_WRONLY);
    if (fd < 0)
    {
        error("[SPI] Failed to open device: {PATH}", "PATH", devPath);
        co_return false;
    }

    ssize_t bytesWritten = write(fd, image, image_size);

    close(fd);

    if (bytesWritten < 0)
    {
        error("[SPI] Failed to write to device");
        co_return false;
    }

    if (static_cast<size_t>(bytesWritten) != image_size)
    {
        error("[SPI] Incomplete write to device");
        co_return false;
    }

    info("[SPI] Successfully wrote {NBYTES} bytes to {PATH}", "NBYTES",
         bytesWritten, "PATH", devPath);

    co_return true;
}

const auto stateOn =
    sdbusplus::common::xyz::openbmc_project::state::Host::HostState::Running;
const auto stateOff =
    sdbusplus::common::xyz::openbmc_project::state::Host::HostState::Off;

const auto transitionOn =
    sdbusplus::common::xyz::openbmc_project::state::Host::Transition::On;
const auto transitionOff =
    sdbusplus::common::xyz::openbmc_project::state::Host::Transition::Off;

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> SPIDevice::setHostPowerstate(
    sdbusplus::async::context& ctx, bool state)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto client = sdbusplus::client::xyz::openbmc_project::state::Host(ctx)
                      .service("xyz.openbmc_project.State.Host")
                      .path("/xyz/openbmc_project/state/host0");

    auto targetState = (state) ? stateOn : stateOff;

    co_await client.requested_host_transition(
        (state) ? transitionOn : transitionOff);

    debug("[PWR] requested host transition to {STATE}", "STATE", targetState);

    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(10));

    const auto actualState = co_await client.current_host_state();

    if (actualState == targetState)
    {
        debug("[PWR] successfully achieved state {STATE}", "STATE",
              targetState);
    }
    else
    {
        debug("[PWR] failed to achieve state {STATE}", "STATE", targetState);
    }

    co_return actualState == targetState;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<std::optional<bool>> SPIDevice::getHostPowerstate(
    sdbusplus::async::context& ctx)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto client = sdbusplus::client::xyz::openbmc_project::state::Host(ctx)
                      .service("xyz.openbmc_project.State.Host")
                      .path("/xyz/openbmc_project/state/host0");

    auto res = co_await client.current_host_state();

    if (res == stateOn)
    {
        co_return true;
    }
    else if (res == stateOff)
    {
        co_return false;
    }

    error("[PWR] unexpected power state: {STATE}", "STATE", res);

    co_return std::nullopt;
}
