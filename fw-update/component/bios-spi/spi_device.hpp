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
    SPIDevice(sdbusplus::async::context& io, const std::string& spiDevName,
              bool dryRun, bool hasME,
              const std::vector<std::string>& gpioLines,
              const std::vector<uint8_t>& gpioValues, uint32_t vendorIANA,
              const std::string& compatible, FWManager* parent);

    sdbusplus::async::task<bool> startUpdateAsync() final;

    sdbusplus::async::task<bool> deviceSpecificUpdateFunction(
        const uint8_t* image, size_t image_size,
        std::shared_ptr<SoftwareActivationProgress> activationProgress) final;

  private:
    // Management Engine specific members and functions
    bool hasManagementEngine;
    sdbusplus::async::task<> setManagementEngineRecoveryMode();
    sdbusplus::async::task<> resetManagementEngine();

    std::vector<std::string> gpioLines;
    std::vector<uint8_t> gpioValues;

    // SPI specific members and functions
    std::string spiDev;

    // @param spi_dev    e.g. "1e630000.spi"
    // @returns          true on success
    sdbusplus::async::task<bool> bindSPIFlash();

    // @param spi_dev    e.g. "1e630000.spi"
    // @returns          true on success
    sdbusplus::async::task<bool> unbindSPIFlash();

    // @param spi_dev    e.g. "1e630000.spi"
    bool isSPIFlashBound();

    sdbusplus::async::task<bool> writeSPIFlash(
        const uint8_t* image, size_t image_size,
        const std::shared_ptr<SoftwareActivationProgress>& activationProgress);

    // this function assumes:
    // - host is powered off
    sdbusplus::async::task<bool>
        writeSPIFlashHostOff(const uint8_t* image, size_t image_size);

    // this function assumes:
    // - host is powered off
    // - gpio / mux is set
    sdbusplus::async::task<bool>
        writeSPIFlashHostOffGPIOSet(const uint8_t* image, size_t image_size);

    // this function assumes:
    // - host is powered off
    // - gpio / mux is set
    // - spi device is bound to the driver
    sdbusplus::async::task<bool> writeSPIFlashHostOffGPIOSetDeviceBound(
        const uint8_t* image, size_t image_size);
};
