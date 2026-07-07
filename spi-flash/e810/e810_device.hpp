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

    /** @brief Returns the cached version, 'Unknown' until the MCTP endpoint
     *         has been discovered and queried.
     */
    std::string getVersion() override;

  private:
    /** @brief Wait for the E810 MCTP endpoint, query its firmware version
     *         and publish it on D-Bus. Retries when endpoints appear.
     */
    sdbusplus::async::task<> refreshVersion();

    /** @returns  the EID of the E810 endpoint, nullopt if not present yet */
    sdbusplus::async::task<std::optional<uint8_t>> getEndpointId();

    /** @brief PLDM GetFirmwareParameters over MCTP
     *  @param eid  endpoint to query
     *  @returns    version string, or nullopt on error
     */
    sdbusplus::async::task<std::optional<std::string>> queryVersion(
        uint8_t eid);

    std::string version = versionUnknown;
    uint8_t instanceId = 0;
};
