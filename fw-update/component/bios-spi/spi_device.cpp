#include "spi_device.hpp"

#include "bios_sw.hpp"
#include "fw-update/common/device.hpp"
#include "fw-update/common/fw_manager.hpp"

#include <boost/asio/steady_timer.hpp>
#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/timer.hpp>

#include <fstream>

using namespace std::literals;

SPIDevice::SPIDevice(sdbusplus::async::context& io, bool dryRun, bool hasME,
                     const std::vector<std::string>& gpioLines,
                     const std::vector<uint8_t>& gpioValues,
                     const std::string& vendorIANA,
                     const std::string& compatible, FWManager* parent) :
    Device(io, dryRun, vendorIANA, compatible, parent),
    hasManagementEngine(hasME), gpioLines(gpioLines), gpioValues(gpioValues)
{
    lg2::debug("initialized SPI Device instance on dbus");
}

void SPIDevice::startUpdateAsync()
{
    lg2::debug("starting the async update");

    // design: generate a new random id for the new fw image.
    // this new swid will then be used for the object path for the new image

    std::string newSwId = FWManager::getRandomSoftwareId();
    std::string objPathStr = FWManager::getObjPathFromSwid(newSwId);
    const char* objPath = objPathStr.c_str();

    // create the new instance of the BiosSW class
    std::shared_ptr<BiosSW> newsw =
        std::make_shared<BiosSW>(parent->io, bus, newSwId, objPath, this);

    softwareUpdate = newsw;

    // this deletes the old sw also
    continueUpdate(image, applyTime, oldSwId);

    softwareCurrent = newsw;
}

void SPIDevice::deviceSpecificUpdateFunction(sdbusplus::message::unix_fd image)
{
    this->writeSPIFlash(image);
}

constexpr const char* IPMB_SERVICE = "xyz.openbmc_project.Ipmi.Channel.Ipmb";
constexpr const char* IPMB_PATH = "/xyz/openbmc_project/Ipmi/Channel/Ipmb";
constexpr const char* IPMB_INTF = "org.openbmc.Ipmb";

void SPIDevice::setManagementEngineRecoveryMode()
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
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

void SPIDevice::resetManagementEngine()
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
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

void SPIDevice::bindSPIFlash(const std::string& spi_dev)
{
    std::ofstream ofbind("/sys/bus/platform/drivers/spi-aspeed-smc/bind",
                         std::ofstream::out);
    ofbind << spi_dev;
    ofbind.close();
    lg2::info("SPI flash bound to SMC");
}

void SPIDevice::unbindSPIFlash(const std::string& spi_dev)
{
    std::ofstream ofunbind("/sys/bus/platform/drivers/spi-aspeed-smc/unbind",
                           std::ofstream::out);
    ofunbind << spi_dev;
    ofunbind.close();
    lg2::info("SPI flash unbound from SMC");
}

void SPIDevice::writeSPIFlash(sdbusplus::message::unix_fd image)
{
    if (!FWManager::setHostPowerstate(false))
    {
        lg2::error("error changing host power state");
        return;
    }

    if (hasManagementEngine)
    {
        setManagementEngineRecoveryMode();
    }

    writeSPIFlashHostOff(image);

    if (hasManagementEngine)
    {
        resetManagementEngine();
    }

    FWManager::setHostPowerstate(true);
}

void SPIDevice::writeSPIFlashHostOff(sdbusplus::message::unix_fd image)
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
        return;
    }

    std::vector<unsigned int> offsets;

    for (const std::string& lineName : gpioLines)
    {
        const ::gpiod::line line = ::gpiod::find_line(lineName);

        if (line.is_used())
        {
            lg2::error("gpio line {LINE} was still used", "LINE", lineName);
            return;
        }
        offsets.push_back(line.offset());
    }

    auto lines = chip.get_lines(offsets);

    ::gpiod::line_request config{"", ::gpiod::line_request::DIRECTION_OUTPUT,
                                 0};
    std::vector<int> values;
    values.reserve(gpioValues.size());

    for (uint8_t value : gpioValues)
    {
        values.push_back(value);
    }

    // select the spi flash via mux / GPIO
    lines.request(config, values);

    writeSPIFlashHostOffGPIOSet(image);

    // switch bios flash back to host via mux / GPIO
    // (assume there is a pull to the default value)
    lines.release();
}

void SPIDevice::writeSPIFlashHostOffGPIOSet(sdbusplus::message::unix_fd image)
{
    SPIDevice::bindSPIFlash(this->spiDev);

    if (dryRun)
    {
        lg2::info("dry run, NOT writing to the chip");
    }
    else
    {
        writeSPIFlashHostOffGPIOSetDeviceBound(image);
    }

    SPIDevice::unbindSPIFlash(this->spiDev);
}

void SPIDevice::writeSPIFlashHostOffGPIOSetDeviceBound(
    sdbusplus::message::unix_fd image)
{
    // call flashrom
    std::filesystem::copy("/proc/self/fd/" + std::to_string(image.fd),
                          "/tmp/image");

    // TODO: just use the mtd device here
    std::string cmd =
        std::format("flashrom -p linux_mtd:dev=6 -w {}", "/tmp/image");
    const int exitcode = system(cmd.c_str());

    if (exitcode != 0)
    {
        lg2::error("flashrom failed to write the image");
    }
}
