#include "interface.hpp"

#include "lattice.hpp"
#include "xo2xo3.hpp"
#include "xo5.hpp"

#include <phosphor-logging/lg2.hpp>

namespace phosphor::software::cpld
{

std::unique_ptr<CpldLatticeManager> getCpldManager(
    sdbusplus::async::context& ctx, const std::string& chipType, uint16_t bus,
    uint8_t address)
{
    auto chip = supportedDeviceMap.find(chipType);
    if (chip->second.updateStrategy == "XO2XO3FamilyUpdate")
    {
        return std::make_unique<XO2XO3FamilyUpdate>(
            ctx, bus, address, chip->second.chipName, "CFG0", false);
    }
    if (chip->second.updateStrategy == "XO5FamilyUpdate")
    {
        return std::make_unique<XO5FamilyUpdate>(
            ctx, bus, address, chip->second.chipName, "CFG0", false);
    }
    lg2::error("Unsupported Lattice CPLD chip: {CHIPTYPE}", "CHIPTYPE",
               chipType);
    return nullptr;
}

sdbusplus::async::task<bool> LatticeCPLD::updateFirmware(
    bool /*force*/, const uint8_t* image, size_t imageSize,
    std::function<bool(int)> progressCallBack)
{
    lg2::info("Updating Lattice CPLD firmware");
    auto cpldManager = getCpldManager(ctx, chipType, bus, address);
    if (cpldManager == nullptr)
    {
        lg2::error("CPLD manager is not initialized.");
        co_return false;
    }
    co_return co_await cpldManager->updateFirmware(image, imageSize,
                                                   progressCallBack);
}

sdbusplus::async::task<bool> LatticeCPLD::getVersion(std::string& version)
{
    lg2::info("Getting Lattice CPLD version");
    auto cpldManager = getCpldManager(ctx, chipType, bus, address);
    if (cpldManager == nullptr)
    {
        lg2::error("CPLD manager is not initialized.");
        co_return false;
    }
    co_return co_await cpldManager->getVersion(version);
}

} // namespace phosphor::software::cpld

// Factory function to create lattice CPLD device
namespace
{
using namespace phosphor::software::cpld;

// Register all the CPLD type with the CPLD factory
const bool vendorRegistered = [] {
    for (const auto& [type, info] : supportedDeviceMap)
    {
        CPLDFactory::instance().registerCPLD(
            type, [type](sdbusplus::async::context& ctx,
                         const std::string& /*chipName*/, uint16_t bus,
                         uint8_t address) {
                // Create and return a LatticeCPLD instance
                // Pass the parameters to the constructor
                return std::make_unique<LatticeCPLD>(ctx, type, bus, address);
            });
    }
    return true;
}();

} // namespace
