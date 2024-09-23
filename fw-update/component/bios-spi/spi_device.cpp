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
                     FWManager* parent) :
    Device(io, dryRun, parent), hasManagementEngine(hasME),
    gpioLines(gpioLines), gpioValues(gpioValues)
{
    lg2::debug("initialized SPI Device instance on dbus");
}

void SPIDevice::startUpdate(sdbusplus::message::unix_fd image,
                            sdbusplus::common::xyz::openbmc_project::software::
                                ApplyTime::RequestedApplyTimes applyTime,
                            const std::string& oldSwId)
{
    this->image = image;
    this->applyTime = applyTime;
    this->oldSwId = oldSwId;

    const int timeout = 5;
    lg2::debug("starting timer for async bios update in {SECONDS}s\n",
               "SECONDS", timeout);

    io.spawn(sdbusplus::async::sleep_for(io, std::chrono::seconds(timeout)) |
             stdexec::then([&]() {
                 (void)this;
                 lg2::debug("update timer expired");
                 startUpdateAsync();
             }));

    lg2::debug("update timer has started");
}

void SPIDevice::startUpdateAsync()
{
    lg2::debug("starting the async update");

    // design: generate a new random id for the new fw image.
    // this new swid will then be used for the object path for the new image

    std::string newSwId = "newswid_version_" + FWManager::getRandomSoftwareId();
    std::string objPathStr = FWManager::getObjPath(newSwId);
    const char* objPath = objPathStr.c_str();

    // TODO
    std::string spiDev = "TODO";

    // create the new instance of the BiosSW class
    std::shared_ptr<BiosSW> newsw =
        std::make_shared<BiosSW>(parent->io, bus, newSwId, objPath, this);

    softwareUpdate = newsw;

    // this deletes the old sw also
    continueUpdate(image, applyTime, oldSwId);

    softwareCurrent = newsw;
}

const auto actNotReady = sdbusplus::common::xyz::openbmc_project::software::
    Activation::Activations::NotReady;
const auto actInvalid = sdbusplus::common::xyz::openbmc_project::software::
    Activation::Activations::Invalid;
const auto actReady = sdbusplus::common::xyz::openbmc_project::software::
    Activation::Activations::Ready;
const auto actActivating = sdbusplus::common::xyz::openbmc_project::software::
    Activation::Activations::Activating;

const auto applyTimeImmediate = sdbusplus::common::xyz::openbmc_project::
    software::ApplyTime::RequestedApplyTimes::Immediate;

void SPIDevice::continueUpdate(
    sdbusplus::message::unix_fd image,
    sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
        RequestedApplyTimes applyTime,
    const std::string& oldSwId)
{
    std::shared_ptr<Software> newsw = softwareUpdate;

    // design: Create Interface xyz.openbmc_project.Software.Activation at
    // ObjectPath with Status = NotReady

    newsw->softwareActivation.activation(actNotReady);

    const bool imageVerificationSuccess = parent->verifyImage(image);

    if (!imageVerificationSuccess)
    {
        lg2::error("failed to verify image");

        newsw->softwareActivation.activation(actInvalid);
        // newsw->activationIntf.activation(actInvalid, false);
        return;
    }

    lg2::info("successfully verified the image");

    newsw->softwareActivation.activation(actReady);

    // design: Create Interface xyz.openbmc_project.Software.Version at
    // ObjectPath this is already done by our class constructor

    // TODO(design): Create Interface
    // xyz.openbmc_project.Software.ActivationProgress at ObjectPath
    // TODO(design): Create Interface
    // xyz.openbmc_project.Software.ActivationBlocksTransition at ObjectPath

    // design: Activation.Status = Activating
    newsw->softwareActivation.activation(actActivating);

    // TODO: take this from EM configuration
    // std::string lineName = "BMC_SPI_SEL";

    // design says we start the update
    this->writeSPIFlash(image);
    // design says we finished the update here
    newsw->softwareActivation.activation(actActive);

    // TODO(design):Delete Interface
    // xyz.openbmc_project.Software.ActivationBlocksTransition
    // TODO(design):Delete Interface
    // xyz.openbmc_project.Software.ActivationProgress

    if (applyTime == applyTimeImmediate)
    {
        // TODO(design): reset device
        // TODO(design): update functional association to system inventory item
        // (instructions unclear)

        // TODO(design): Create Interface xyz.openbmc_project.Software.Update at
        // ObjectPath
        //  this is already done by our class constructor, but we should defer
        //  that to later since this sw has only been applied at this point in
        //  the code

        // design: Delete all interfaces on previous ObjectPath
        // makes sense, we do not want the old sw version to be updated or
        // displayed, since it is not the active version

        deleteOldSw(oldSwId);

        // TODO(design): Create active association to System Inventory Item
    }
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
