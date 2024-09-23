#include "spi_device.hpp"

#include "bios_sw.hpp"
#include "fw-update/common/device.hpp"
#include "fw-update/common/fw_manager.hpp"

#include <boost/asio/steady_timer.hpp>
#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/timer.hpp>

#include <fstream>

SPIDevice::SPIDevice(sdbusplus::async::context& io,
                     const std::string& serviceName, bool dryRun,
                     FWManager* parent) :
    Device(io, serviceName, dryRun, parent)
{
    lg2::debug("initialized SPI Device instance on dbus");
}

void SPIDevice::startUpdate(sdbusplus::message::unix_fd image,
                            sdbusplus::common::xyz::openbmc_project::software::
                                ApplyTime::RequestedApplyTimes applyTime,
                            const std::string& oldSwId)
{
    /*
    const int status = verifyPLDMPackage(image);
    if (status != 0)
    {
        return;
    }
    */

    this->image = image;
    this->applyTime = applyTime;
    this->oldSwId = oldSwId;

    const int timeout = 7;
    lg2::debug("starting timer for async bios update in {SECONDS}s\n",
               "SECONDS", timeout);

    updateTimer.start(std::chrono::seconds(timeout));
}

void SPIDevice::startUpdateAsync()
{
    // design: generate a new random id for the new fw image.
    // this new swid will then be used for the object path for the new image

    std::string newSwId = "newswid_version_" + FWManager::getRandomSoftwareId();
    std::string objPathStr = FWManager::getObjPath(newSwId);
    const char* objPath = objPathStr.c_str();

    // TODO
    std::string spiDev = "TODO";

    // create the new instance of the BiosSW class
    std::shared_ptr<BiosSW> newsw =
        std::make_shared<BiosSW>(bus, newSwId, objPath, this);

    softwareUpdate = newsw;

    continueUpdate(image, applyTime, oldSwId);
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
    newsw->activationIntf.activation(actNotReady, false);

    const bool imageVerificationSuccess = parent->verifyImage(image);

    if (!imageVerificationSuccess)
    {
        lg2::error("failed to verify image");
        newsw->activationIntf.activation(actInvalid, false);
        return;
    }

    lg2::info("successfully verified the image");

    newsw->activationIntf.activation(actReady, false);

    // design: Create Interface xyz.openbmc_project.Software.Version at
    // ObjectPath this is already done by our class constructor

    // TODO(design): Create Interface
    // xyz.openbmc_project.Software.ActivationProgress at ObjectPath
    // TODO(design): Create Interface
    // xyz.openbmc_project.Software.ActivationBlocksTransition at ObjectPath

    // design: Activation.Status = Activating
    newsw->activationIntf.activation(actActivating, false);

    // TODO: take this from EM configuration
    std::string lineName = "BMC_SPI_SEL";

    // design says we start the update
    this->writeSPIFlash(image, lineName);
    // design says we finished the update here
    newsw->activationIntf.activation(actActive, false);

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

void SPIDevice::writeSPIFlash(sdbusplus::message::unix_fd image,
                              const std::string& lineName)
{
    if (!FWManager::setHostPowerstate(false))
    {
        lg2::error("error changing host power state");
        return;
    }

    gpiod::chip chip("/sys/bus/gpio/devices/gpiochip0");
    // TODO: make it work for multiple chips
    // TODO: make it work for multiple gpio lines

    const ::gpiod::line line = ::gpiod::find_line(lineName);

    // gpiod::line_info line = chip.get_line_info(offset);

    if (line.is_used())
    {
        lg2::error("gpio line was still used");
        return;
    }

    std::vector<unsigned int> offsets = {line.offset()};
    auto lines = chip.get_lines(offsets);

    ::gpiod::line_request config{"", ::gpiod::line_request::DIRECTION_OUTPUT,
                                 0};
    std::vector<int> values = {0};

    // select the spi flash via mux / GPIO
    lines.request(config, values);

    if (dryRun)
    {
        lg2::info("dry run, NOT writing to the chip");
        lines.release();
        FWManager::setHostPowerstate(true);
        return;
    }

    SPIDevice::bindSPIFlash(this->spiDev);

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

    SPIDevice::unbindSPIFlash(this->spiDev);

    // switch bios flash back to host via mux / GPIO
    // (assume there is a pull to the default value)
    lines.release();

    FWManager::setHostPowerstate(true);
}
