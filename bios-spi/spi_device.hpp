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

enum FLASH_LAYOUT
{
    FLASH_LAYOUT_FLAT,
    FLASH_LAYOUT_INTEL_FLASH_DESCRIPTOR,
};

enum FLASH_TOOL
{
    FLASH_TOOL_NONE,     // write directly to the mtd device
    FLASH_TOOL_FLASHROM, // use flashrom, to handle e.g. IFD
};

class SPIDevice : public Device, public NotifyWatchIntf
{
  public:
    using Device::softwareCurrent;
    SPIDevice(sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
              uint64_t spiDeviceIndex, bool dryRun, bool hasME,
              const std::vector<std::string>& gpioLinesIn,
              const std::vector<uint64_t>& gpioValuesIn, SoftwareConfig& config,
              SoftwareManager* parent, enum FLASH_LAYOUT layout,
              enum FLASH_TOOL tool);

    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) final;

    // @returns       the bios version which is externally provided.
    static std::string getVersion();

    /** @brief Process async changes to cable configuration */
    auto processVersionUpdate(std::string configFileName)
        -> sdbusplus::async::task<>;

  private:
    bool dryRun;

    // Management Engine specific members and functions
    bool hasManagementEngine;
    sdbusplus::async::task<> setManagementEngineRecoveryMode();
    sdbusplus::async::task<> resetManagementEngine();

    std::vector<std::string> gpioLines;

    std::vector<int> gpioValues;

    uint64_t spiControllerIndex;
    uint64_t spiDeviceIndex;

    enum FLASH_LAYOUT layout;

    enum FLASH_TOOL tool;

    // @param spi_dev    e.g. "1e630000.spi"
    // @returns          true on success
    sdbusplus::async::task<bool> bindSPIFlash();

    // @param spi_dev    e.g. "1e630000.spi"
    // @returns          true on success
    sdbusplus::async::task<bool> unbindSPIFlash();

    // @param spi_dev    e.g. "1e630000.spi"
    static bool isSPIFlashBound();

    // @description preconditions:
    // - host is powered off
    // @returns   true on success
    sdbusplus::async::task<bool> writeSPIFlash(const uint8_t* image,
                                               size_t image_size);

    // @description preconditions:
    // - host is powered off
    // - gpio / mux is set
    // - spi device is bound to the driver
    // we write the flat image here
    // @param image           the component image
    // @param image_size      size of 'image'
    // @returns               true on success
    sdbusplus::async::task<bool> writeSPIFlashDefault(const uint8_t* image,
                                                      size_t image_size);

    // @description preconditions:
    // - host is powered off
    // - gpio / mux is set
    // - spi device is bound to the driver
    // we use 'flashrom' here to write the image since it can deal with
    // Intel Flash Descriptor
    // @param image           the component image
    // @param image_size      size of 'image'
    // @returns               true on success
    sdbusplus::async::task<bool> writeSPIFlashWithFlashrom(
        const uint8_t* image, size_t image_size) const;

    // @returns nullopt on error
    std::optional<std::string> getMTDDevicePath() const;
};
