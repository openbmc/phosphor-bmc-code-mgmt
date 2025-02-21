#pragma once

#include "common/include/device.hpp"
#include "common/include/software.hpp"
#include "common/include/software_manager.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>

#include <string>

using namespace phosphor::software;
using namespace phosphor::software::manager;

class SPIDevice : public Device
{
  public:
    using Device::softwareCurrent;
    SPIDevice(sdbusplus::async::context& ctx, const std::string& spiDevName,
              bool dryRun, bool hasME,
              const std::vector<std::string>& gpioLines,
              const std::vector<uint8_t>& gpioValues, SoftwareConfig& config,
              SoftwareManager* parent, bool layoutFlat, bool toolFlashrom);

    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) final;

  private:
    bool dryRun;

    // Management Engine specific members and functions
    bool hasManagementEngine;
    sdbusplus::async::task<> setManagementEngineRecoveryMode();
    sdbusplus::async::task<> resetManagementEngine();

    std::vector<std::string> gpioLines;
    std::vector<uint8_t> gpioValues;

    // SPI specific members and functions
    std::string spiDev;

    // does the spi flash have a flat layout?
    // Otherwise, we have to use Intel Flash Descriptor
    // or another descriptor to figure out which regions should be written
    bool layoutFlat;

    // do we use flashrom?
    // if not, write directly to the mtd device.
    bool toolFlashrom;

    // @param spi_dev    e.g. "1e630000.spi"
    // @returns          true on success
    sdbusplus::async::task<bool> bindSPIFlash();

    // @param spi_dev    e.g. "1e630000.spi"
    // @returns          true on success
    sdbusplus::async::task<bool> unbindSPIFlash();

    // @param spi_dev    e.g. "1e630000.spi"
    bool isSPIFlashBound();

    sdbusplus::async::task<bool> writeSPIFlash(const uint8_t* image,
                                               size_t image_size);

    // this function assumes:
    // - host is powered off
    sdbusplus::async::task<bool> writeSPIFlashHostOff(const uint8_t* image,
                                                      size_t image_size);

    // this function assumes:
    // - host is powered off
    // - gpio / mux is set
    sdbusplus::async::task<bool> writeSPIFlashHostOffGPIOSet(
        const uint8_t* image, size_t image_size);

    // this function assumes:
    // - host is powered off
    // - gpio / mux is set
    // - spi device is bound to the driver
    // we write the flat image here
    // @param image           the component image
    // @param image_size      size of 'image'
    // @returns               true on success
    static sdbusplus::async::task<bool> writeSPIFlashHostOffGPIOSetDeviceBound(
        const uint8_t* image, size_t image_size);

    // this function assumes:
    // - host is powered off
    // - gpio / mux is set
    // - spi device is bound to the driver
    // we use 'flashrom' here to write the image since it can deal with
    // Intel Flash Descriptor
    // TODO: look into using libflashrom instead
    // @param image           the component image
    // @param image_size      size of 'image'
    // @returns               true on success
    sdbusplus::async::task<bool> writeSPIFlashFlashromHostOffGPIOSetDeviceBound(
        const uint8_t* image, size_t image_size) const;

    // @param state   desired powerstate (true means on)
    // @returns       true on success
    static sdbusplus::async::task<bool> setHostPowerstate(
        sdbusplus::async::context& ctx, bool state);

    // @returns       true when powered
    // @returns       std::nullopt on failure to query power state
    static sdbusplus::async::task<std::optional<bool>> getHostPowerstate(
        sdbusplus::async::context& ctx);
};
