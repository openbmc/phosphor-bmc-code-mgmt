#include "spi_factory.hpp"

#include "bios/bios_device.hpp"
#include "e810/e810_device.hpp"

namespace phosphor::software::manager
{

SPIFactory& SPIFactory::instance()
{
    static SPIFactory factory;
    return factory;
}

std::unique_ptr<SPIDevice> SPIFactory::create(
    const std::string& chipType, sdbusplus::async::context& ctx,
    uint64_t spiControllerIndex, uint64_t spiDeviceIndex, bool dryRun,
    const std::vector<std::string>& names, const std::vector<bool>& values,
    SoftwareConfig& config, SoftwareManager* parent)
{
    try
    {
        if (chipType == getSpiTypeStr(spiChip::INTEL_HOST_BIOS) ||
            chipType == getSpiTypeStr(spiChip::HOST_BIOS))
        {
            return std::make_unique<BIOSDevice>(
                ctx, spiControllerIndex, spiDeviceIndex, dryRun, names, values,
                config, parent);
        }

        if (chipType == getSpiTypeStr(spiChip::INTEL_E810_NIC))
        {
            return std::make_unique<E810Device>(
                ctx, spiControllerIndex, spiDeviceIndex, dryRun, names, values,
                config, parent);
        }
    }
    catch (const std::exception& e)
    {
        error("Failed to create SPI device '{TYPE}': {ERROR}", "TYPE", chipType,
              "ERROR", e.what());
        return nullptr;
    }

    error("Unsupported SPI device type: {TYPE}", "TYPE", chipType);
    return nullptr;
}

std::vector<std::string> SPIFactory::getConfigInterfaceNames()
{
    std::vector<std::string> configs;
    configs.reserve(supportedSpiChips.size());
    for (const auto& chipEnum : supportedSpiChips)
    {
        configs.push_back(getSpiTypeStr(chipEnum));
    }
    return configs;
}

} // namespace phosphor::software::manager
