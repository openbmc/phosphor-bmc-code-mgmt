#pragma once

#include "common/include/NotifyWatch.hpp"
#include "common/include/device.hpp"
#include "common/include/software.hpp"
#include "common/include/software_manager.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>

#include <string>

class SPIDevice;

using namespace phosphor::software;
using namespace phosphor::software::manager;
using namespace phosphor::notify::watch;

using NotifyWatchIntf = phosphor::notify::watch::NotifyWatch<SPIDevice>;

const std::string biosVersionDirPath = "/var/bios/";
const std::string biosVersionFilename = "host0_bios_version.txt";
const std::string biosVersionPath = biosVersionDirPath + biosVersionFilename;

const std::string versionUnknown = "Unknown";

class SPIDevice : public Device, public NotifyWatchIntf
{
  public:
    using Device::softwareCurrent;
    SPIDevice(sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
              uint64_t spiDeviceIndex, bool dryRun,
              const std::vector<std::string>& gpioLines,
              const std::vector<uint8_t>& gpioValues, SoftwareConfig& config,
              SoftwareManager* parent, bool layoutFlat, bool toolFlashrom);

    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) final;

    // @returns       the bios version which is externally provided.
    static std::string getVersion();

    /** @brief Process async changes to cable configuration */
    auto processVersionUpdate(std::string configFileName)
        -> sdbusplus::async::task<>;

  private:
    bool dryRun;

    std::vector<std::string> gpioLines;
    std::vector<uint8_t> gpioValues;

    // SPI specific members and functions
    uint64_t spiControllerIndex;
    uint64_t spiDeviceIndex;

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
    static bool isSPIFlashBound();

    // this function assumes:
    // - host is powered off
    // @returns   true on success
    sdbusplus::async::task<bool> writeSPIFlash(const uint8_t* image,
                                               size_t image_size);

    // this function assumes:
    // - host is powered off
    // - gpio / mux is set
    // - spi device is bound to the driver
    // we write the flat image here
    // @param image           the component image
    // @param image_size      size of 'image'
    // @returns               true on success
    static sdbusplus::async::task<bool> writeSPIFlashDefault(
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
    sdbusplus::async::task<bool> writeSPIFlashWithFlashrom(
        const uint8_t* image, size_t image_size) const;
};
