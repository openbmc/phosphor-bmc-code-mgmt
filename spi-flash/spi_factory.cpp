#include "spi_factory.hpp"

#include "bios/bios_device.hpp"

#include <phosphor-logging/lg2.hpp>

namespace phosphor::software::manager
{

SPIFactory& SPIFactory::instance()
{
    static SPIFactory factory;
    return factory;
}

void SPIFactory::registerSPI(const std::string& chipType, Creator creator)
{
    creators[chipType] = std::move(creator);
}

std::unique_ptr<SPIDevice> SPIFactory::create(
    const std::string& chipType, sdbusplus::async::context& ctx,
    uint64_t spiControllerIndex, uint64_t spiDeviceIndex, bool dryRun,
    const std::vector<std::string>& names, const std::vector<bool>& values,
    SoftwareConfig& config, SoftwareManager* parent, enum FlashLayout layout,
    enum FlashTool tool) const
{
    auto it = creators.find(chipType);
    if (it != creators.end())
    {
        return (it->second)(ctx, spiControllerIndex, spiDeviceIndex, dryRun,
                            names, values, config, parent, layout, tool);
    }
    return nullptr;
}

std::vector<std::string> SPIFactory::getConfigs() const
{
    std::vector<std::string> configs;
    configs.reserve(creators.size());
    for (const auto& pair : creators)
    {
        configs.push_back(pair.first);
    }
    return configs;
}

} // namespace phosphor::software::manager

namespace
{
using namespace phosphor::software::manager;

const bool vendorRegistered = [] {
    for (const auto& chipEnum : supportedSpiChips)
    {
        auto typeStr = getSpiTypeStr(chipEnum);

        SPIFactory::instance().registerSPI(
            typeStr,
            [chipEnum](sdbusplus::async::context& ctx, uint64_t ctrlIdx,
                       uint64_t devIdx, bool dryRun,
                       const std::vector<std::string>& names,
                       const std::vector<bool>& values, SoftwareConfig& config,
                       SoftwareManager* parent, FlashLayout layout,
                       FlashTool tool) -> std::unique_ptr<SPIDevice> {
                switch (chipEnum)
                {
                    case spiChip::HOST_BIOS:
                        return std::make_unique<BIOSDevice>(
                            ctx, ctrlIdx, devIdx, dryRun, names, values, config,
                            parent, layout, tool, "/var/bios/",
                            "host0_bios_version.txt");

                    default:
                        lg2::error("Unsupported SPI Chip Enum: {ENUM}", "ENUM",
                                   static_cast<int>(chipEnum));
                        return nullptr;
                }
            });
    }
    return true;
}();

} // namespace
