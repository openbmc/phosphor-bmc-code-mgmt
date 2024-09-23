#pragma once

#include "fw-update/common/device.hpp"
#include "fw-update/common/fw_manager.hpp"

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/timer.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

#include <string>

class Software;

class SPIDevice : public Device
{
  public:
    SPIDevice(sdbusplus::async::context& io, bool dryRun, bool hasME,
              const std::vector<std::string>& gpioLines,
              const std::vector<uint8_t>& gpioValues,
              const std::string& vendorIANA, const std::string& compatible,
              FWManager* parent);

    sdbusplus::async::task<bool> startUpdateAsync() final;

    sdbusplus::async::task<bool> deviceSpecificUpdateFunction(
        sdbusplus::message::unix_fd image,
        std::shared_ptr<SoftwareActivationProgress> activationProgress) final;

  private:
    // Management Engine specific members and functions
    bool hasManagementEngine;
    void setManagementEngineRecoveryMode();
    void resetManagementEngine();

    std::vector<std::string> gpioLines;
    std::vector<uint8_t> gpioValues;

    // SPI specific members and functions
    std::string spiDev;

    static void bindSPIFlash(const std::string& spi_dev);
    static void unbindSPIFlash(const std::string& spi_dev);
    sdbusplus::async::task<bool> writeSPIFlash(
        sdbusplus::message::unix_fd image,
        const std::shared_ptr<SoftwareActivationProgress>& activationProgress);

    // this function assumes:
    // - host is powered off
    void writeSPIFlashHostOff(sdbusplus::message::unix_fd image);

    // this function assumes:
    // - host is powered off
    // - gpio / mux is set
    void writeSPIFlashHostOffGPIOSet(sdbusplus::message::unix_fd image);

    // this function assumes:
    // - host is powered off
    // - gpio / mux is set
    // - spi device is bound to the driver
    static void writeSPIFlashHostOffGPIOSetDeviceBound(
        sdbusplus::message::unix_fd image);
};
