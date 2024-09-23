#include "spi_device.hpp"

#include "fw-update/common/include/device.hpp"
#include "fw-update/common/include/fw_manager.hpp"

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>

#include <fstream>

using namespace std::literals;

SPIDevice::SPIDevice(
    sdbusplus::async::context& io, const std::string& spiDevName, bool dryRun,
    bool hasME, const std::vector<std::string>& gpioLines,
    const std::vector<uint8_t>& gpioValues, uint32_t vendorIANA,
    const std::string& compatible, FWManager* parent,
    const std::string& objPathConfig, bool layoutFlat, bool toolFlashrom) :
    Device(io, dryRun, vendorIANA, compatible, objPathConfig, parent),
    hasManagementEngine(hasME), gpioLines(gpioLines), gpioValues(gpioValues),
    spiDev(spiDevName), layoutFlat(layoutFlat), toolFlashrom(toolFlashrom)
{
    lg2::debug("initialized SPI Device instance on dbus");
}

// NOLINTBEGIN
sdbusplus::async::task<bool> SPIDevice::deviceSpecificUpdateFunction(
    const uint8_t* image, size_t image_size,
    std::unique_ptr<SoftwareActivationProgress>& activationProgress)
// NOLINTEND
{
    // NOLINTBEGIN
    bool success =
        co_await this->writeSPIFlash(image, image_size, activationProgress);
    // NOLINTEND
    co_return success;
}

constexpr const char* IPMB_SERVICE = "xyz.openbmc_project.Ipmi.Channel.Ipmb";
constexpr const char* IPMB_PATH = "/xyz/openbmc_project/Ipmi/Channel/Ipmb";
constexpr const char* IPMB_INTF = "org.openbmc.Ipmb";

// NOLINTBEGIN
sdbusplus::async::task<> SPIDevice::setManagementEngineRecoveryMode()
// NOLINTEND
{
    lg2::info("[ME] setting Management Engine to recovery mode");
    auto m =
        bus.new_method_call(IPMB_SERVICE, IPMB_PATH, IPMB_INTF, "sendRequest");

    // me address, 0x2e oen, 0x00 - lun, 0xdf - force recovery
    uint8_t cmd_recover[] = {0x1, 0x2e, 0x0, 0xdf};
    for (unsigned int i = 0; i < sizeof(cmd_recover); i++)
    {
        m.append(cmd_recover[i]);
    }
    std::vector<uint8_t> remainder = {0x04, 0x57, 0x01, 0x00, 0x01};
    m.append(remainder);

    auto reply = m.call();
    (void)reply;

    co_await sdbusplus::async::sleep_for(io, std::chrono::seconds(5));

    co_return;
}

// NOLINTBEGIN
sdbusplus::async::task<> SPIDevice::resetManagementEngine()
// NOLINTEND
{
    lg2::info("[ME] resetting Management Engine");
    auto m =
        bus.new_method_call(IPMB_SERVICE, IPMB_PATH, IPMB_INTF, "sendRequest");

    // me address, 0x6 App Fn, 0x00 - lun, 0x2 - cold reset
    uint8_t cmd_recover[] = {0x1, 0x6, 0x0, 0x2};
    for (unsigned int i = 0; i < sizeof(cmd_recover); i++)
    {
        m.append(cmd_recover[i]);
    }
    std::vector<uint8_t> remainder;
    m.append(remainder);

    auto reply = m.call();
    (void)reply;
    co_await sdbusplus::async::sleep_for(io, std::chrono::seconds(5));

    co_return;
}

const std::string spiAspeedSMCPath = "/sys/bus/platform/drivers/spi-aspeed-smc";

// NOLINTBEGIN
sdbusplus::async::task<bool> SPIDevice::bindSPIFlash()
// NOLINTEND
{
    lg2::info("[SPI] binding flash to SMC");
    std::ofstream ofbind(spiAspeedSMCPath + "/bind", std::ofstream::out);
    ofbind << this->spiDev;
    ofbind.close();

    // wait for kernel
    co_await sdbusplus::async::sleep_for(io, std::chrono::seconds(2));

    co_return isSPIFlashBound();
}

// NOLINTBEGIN
sdbusplus::async::task<bool> SPIDevice::unbindSPIFlash()
// NOLINTEND
{
    lg2::info("[SPI] unbinding flash from SMC");
    std::ofstream ofunbind(spiAspeedSMCPath + "/unbind", std::ofstream::out);
    ofunbind << this->spiDev;
    ofunbind.close();

    // wait for kernel
    co_await sdbusplus::async::sleep_for(io, std::chrono::seconds(2));

    co_return !isSPIFlashBound();
}

bool SPIDevice::isSPIFlashBound()
{
    std::string path = spiAspeedSMCPath + "/" + this->spiDev;
    lg2::debug("[SPI] checking {PATH}", "PATH", path);

    return std::filesystem::exists(path);
}

// NOLINTBEGIN
sdbusplus::async::task<bool> SPIDevice::writeSPIFlash(
    const uint8_t* image, size_t image_size,
    const std::unique_ptr<SoftwareActivationProgress>& activationProgress)
// NOLINTEND
{
    auto currentPowerstateOpt = co_await parent->getHostPowerstate();

    if (!currentPowerstateOpt.has_value())
    {
        co_return false;
    }

    const bool prevPowerstate = currentPowerstateOpt.value();

    // NOLINTBEGIN
    bool success = co_await parent->setHostPowerstate(false);
    // NOLINTEND
    if (!success)
    {
        lg2::error("error changing host power state");
        co_return false;
    }
    activationProgress->progress(10);

    if (hasManagementEngine)
    {
        co_await setManagementEngineRecoveryMode();
    }
    activationProgress->progress(20);

    success = co_await writeSPIFlashHostOff(image, image_size);

    if (success)
    {
        activationProgress->progress(70);
    }

    if (hasManagementEngine)
    {
        co_await resetManagementEngine();
    }

    if (success)
    {
        activationProgress->progress(90);
    }

    // restore the previous powerstate
    const bool powerstate_restore =
        co_await parent->setHostPowerstate(prevPowerstate);
    if (!powerstate_restore)
    {
        lg2::error("error changing host power state");
        co_return false;
    }

    // return value here is only describing if we successfully wrote to the
    // SPI flash. Restoring powerstate can still fail.
    co_return success;
}

// NOLINTBEGIN
sdbusplus::async::task<bool>
    SPIDevice::writeSPIFlashHostOff(const uint8_t* image, size_t image_size)
// NOLINTEND
{
    gpiod::chip chip;
    try
    {
        // TODO: make it work for multiple chips
        chip = gpiod::chip("/dev/gpiochip0");
    }
    catch (std::exception& e)
    {
        lg2::error(e.what());
        co_return false;
    }

    std::vector<unsigned int> offsets;

    for (const std::string& lineName : gpioLines)
    {
        const ::gpiod::line line = ::gpiod::find_line(lineName);

        if (line.is_used())
        {
            lg2::error("gpio line {LINE} was still used", "LINE", lineName);
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

    lg2::debug("[gpio] requesting gpios to mux SPI to BMC");
    lines.request(config, values);

    co_await writeSPIFlashHostOffGPIOSet(image, image_size);

    lines.release();

    // switch bios flash back to host via mux / GPIO
    // (not assume there is a pull to the default value)
    lg2::debug("[gpio] requesting gpios to mux SPI to Host");
    lines.request(config, valuesInverted);

    lines.release();

    co_return true;
}

// NOLINTBEGIN
sdbusplus::async::task<bool> SPIDevice::writeSPIFlashHostOffGPIOSet(
    const uint8_t* image, size_t image_size)
// NOLINTEND
{
    bool success = true;

    if (SPIDevice::isSPIFlashBound())
    {
        lg2::debug("[SPI] flash was already bound, unbinding it now");
        success = co_await SPIDevice::unbindSPIFlash();

        if (!success)
        {
            lg2::error("[SPI] error unbinding spi flash");
            co_return false;
        }
    }

    success = co_await SPIDevice::bindSPIFlash();

    if (!success)
    {
        lg2::error("[SPI] failed to bind spi device");
        co_await SPIDevice::unbindSPIFlash();
        co_return false;
    }

    if (dryRun)
    {
        lg2::info("[SPI] dry run, NOT writing to the chip");
    }
    else
    {
        if (this->toolFlashrom)
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

// NOLINTBEGIN
sdbusplus::async::task<bool>
    SPIDevice::writeSPIFlashFlashromHostOffGPIOSetDeviceBound(
        const uint8_t* image, size_t image_size)
// NOLINTEND
{
    // TODO: randomize the name to enable parallel updates
    std::string path = "/tmp/spi-device-image.bin";
    int fd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0)
    {
        lg2::error("[SPI] Failed to open file: {PATH}", "PATH", path);
        co_return false;
    }

    const ssize_t bytesWritten = write(fd, image, image_size);

    close(fd);

    if (bytesWritten < 0 || static_cast<size_t>(bytesWritten) != image_size)
    {
        lg2::error("[SPI] Failed to write image to file");
        co_return false;
    }

    // TODO: do not hardcode the mtd device
    std::string cmd = "flashrom -p linux_mtd:dev=6 ";

    if (this->layoutFlat)
    {
        cmd += "-w " + path;
    }
    else
    {
        cmd += "--ifd -i fd -i bios -i me -w " + path;
    }

    lg2::info("[flashrom] running {CMD}", "CMD", cmd);

    const int exitCode = std::system(cmd.c_str());

    if (exitCode != 0)
    {
        lg2::error("[SPI] error running flaashrom");
    }

    // in debug mode we do not delete the raw component image
    if (!parent->debug)
    {
        std::filesystem::remove(path);
    }

    co_return exitCode;
}

// NOLINTBEGIN
sdbusplus::async::task<bool> SPIDevice::writeSPIFlashHostOffGPIOSetDeviceBound(
    const uint8_t* image, size_t image_size)
// NOLINTEND
{
    // TODO: not hardcode the mtd device
    std::string devPath = "/dev/mtd6";
    int fd = open(devPath.c_str(), O_WRONLY);
    if (fd < 0)
    {
        lg2::error("[SPI] Failed to open device: {PATH}", "PATH", devPath);
        co_return false;
    }

    ssize_t bytesWritten = write(fd, image, image_size);

    close(fd);

    if (bytesWritten < 0)
    {
        lg2::error("[SPI] Failed to write to device");
        co_return false;
    }

    if (static_cast<size_t>(bytesWritten) != image_size)
    {
        lg2::error("[SPI] Incomplete write to device");
        co_return false;
    }

    lg2::info("[SPI] Successfully wrote {NBYTES} bytes to {PATH}", "NBYTES",
              bytesWritten, "PATH", devPath);

    co_return true;
}
