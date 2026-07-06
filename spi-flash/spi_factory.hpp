#pragma once

#include "spi_device.hpp"

#include <sdbusplus/async.hpp>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace phosphor::software::manager
{

enum class spiChip
{
    HOST_BIOS,
    UNSUPPORTED = -1,
};

inline std::string getSpiTypeStr(spiChip chip)
{
    static const std::unordered_map<spiChip, std::string> chipStringMap = {
        {spiChip::HOST_BIOS, "HOSTSPIFlash"},
    };

    auto it = chipStringMap.find(chip);
    if (it == chipStringMap.end())
    {
        lg2::error("Unsupported SPI chip enum: {CHIPENUM}", "CHIPENUM",
                   static_cast<int>(chip));
        return "";
    }
    return it->second;
}

const std::vector<spiChip> supportedSpiChips = {
    spiChip::HOST_BIOS,
};

class SPIFactory
{
  public:
    using Creator = std::function<std::unique_ptr<SPIDevice>(
        sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
        uint64_t spiDeviceIndex, bool dryRun,
        const std::vector<std::string>& names, const std::vector<bool>& values,
        SoftwareConfig& config, SoftwareManager* parent,
        enum FlashLayout layout, enum FlashTool tool)>;

    static SPIFactory& instance();

    void registerSPI(const std::string& chipType, Creator creator);

    std::unique_ptr<SPIDevice> create(
        const std::string& chipType, sdbusplus::async::context& ctx,
        uint64_t spiControllerIndex, uint64_t spiDeviceIndex, bool dryRun,
        const std::vector<std::string>& names, const std::vector<bool>& values,
        SoftwareConfig& config, SoftwareManager* parent,
        enum FlashLayout layout, enum FlashTool tool) const;

    std::vector<std::string> getConfigs() const;

  private:
    std::map<std::string, Creator> creators;
};

} // namespace phosphor::software::manager
