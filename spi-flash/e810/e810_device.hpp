#pragma once
#include "spi_device.hpp"

using namespace phosphor::software;
using namespace phosphor::software::manager;

class E810Device : public SPIDevice
{
  public:
    E810Device(sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
               uint64_t spiDeviceIndex, bool dryRun,
               const std::vector<std::string>& gpioLinesIn,
               const std::vector<bool>& gpioValuesIn, SoftwareConfig& config,
               SoftwareManager* parent, enum FlashLayout layout,
               enum FlashTool tool);

    std::string getVersion() override;
};
