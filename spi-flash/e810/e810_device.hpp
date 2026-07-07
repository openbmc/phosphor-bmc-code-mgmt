#pragma once

#include "spi_device.hpp"

#include <sdbusplus/async.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace phosphor::software;
using namespace phosphor::software::manager;

class E810Device : public SPIDevice
{
  public:
    E810Device(sdbusplus::async::context& ctx, uint64_t inSpiControllerIndex,
               uint64_t inSpiDeviceIndex, bool inDryRun,
               const std::vector<std::string>& inGpioLines,
               const std::vector<bool>& inGpioValues, SoftwareConfig& inConfig,
               SoftwareManager* inParent);

    /** @brief Returns the cached version, 'Unknown' until the endpoint has
     *         been discovered and queried.
     */
    std::string getVersion() override;

  private:
    /** @brief Resolve the E810 endpoint and publish its firmware version. */
    sdbusplus::async::task<> refreshVersion();

    /** @brief PLDM GetFirmwareParameters against the given endpoint. */
    sdbusplus::async::task<std::optional<std::string>> queryVersion(
        uint8_t eid);

    std::string version = versionUnknown;
    uint8_t instanceId = 0;
};
