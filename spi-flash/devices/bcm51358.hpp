#pragma once

#include "common/include/serial/serial_terminal.hpp"
#include "spi_device.hpp"

using namespace phosphor::software::serial;

class BCM51358Device : public SPIDevice, public SerialTerminal
{
  public:
    BCM51358Device(sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
                   uint64_t spiDeviceIndex, bool dryRun, GPIOGroup&& muxGPIO,
                   SoftwareConfig& config, SoftwareManager* parent,
                   const std::string& port, const uint32_t baud) :
        SPIDevice(ctx, spiControllerIndex, spiDeviceIndex, dryRun,
                  std::move(muxGPIO), config, parent, flashLayoutFlat,
                  flashToolFlashcp),
        SerialTerminal(config.configType, port, baud, "CMD> ")
    {}

    std::string getVersion() final;
};
