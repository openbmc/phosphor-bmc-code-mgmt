#pragma once

#include "spi_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::manager
{

enum class spiChip
{
    INTEL_HOST_BIOS,
    HOST_BIOS,
    INTEL_E810_NIC,
    UNSUPPORTED = -1,
};

const std::vector<spiChip> supportedSpiChips = {
    spiChip::INTEL_HOST_BIOS,
    spiChip::HOST_BIOS,
    spiChip::INTEL_E810_NIC,
};

inline std::string getSpiTypeStr(spiChip chip)
{
    static const std::unordered_map<spiChip, std::string> chipStringMap = {
        {spiChip::INTEL_HOST_BIOS, "IntelHostSPIFlash"},
        {spiChip::HOST_BIOS, "HostSPIFlash"},
        {spiChip::INTEL_E810_NIC, "IntelE810SPIFlash"}};

    auto it = chipStringMap.find(chip);
    if (it == chipStringMap.end())
    {
        error("Unsupported SPI chip enum: {CHIPENUM}", "CHIPENUM",
              static_cast<int>(chip));
        return "";
    }
    return it->second;
}

class SPIFactory
{
  public:
    using Creator = std::function<std::unique_ptr<SPIDevice>(
        sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
        uint64_t spiDeviceIndex, bool dryRun,
        const std::vector<std::string>& names, const std::vector<bool>& values,
        SoftwareConfig& config, SoftwareManager* parent)>;

    static SPIFactory& instance();

    void registerSPI(const std::string& chipType, Creator creator);

    std::unique_ptr<SPIDevice> create(
        const std::string& chipType, sdbusplus::async::context& ctx,
        uint64_t spiControllerIndex, uint64_t spiDeviceIndex, bool dryRun,
        const std::vector<std::string>& names, const std::vector<bool>& values,
        SoftwareConfig& config, SoftwareManager* parent) const;

    std::vector<std::string> getConfigs() const;

  private:
    std::map<std::string, Creator> creators;
};

} // namespace phosphor::software::manager
