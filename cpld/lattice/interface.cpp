#include "interface.hpp"

#include "lattice.hpp"

#include <phosphor-logging/lg2.hpp>

namespace phosphor::software::cpld
{

const std::vector<std::string> supportedTypes = {
    "LatticeMachXO2Firmware",
    "LatticeMachXO3Firmware",
};

sdbusplus::async::task<bool> LatticeCPLD::updateFirmware(
    bool /*force*/, const uint8_t* image, size_t imageSize,
    std::function<bool(int)> progressCallBack)
{
    lg2::info("Updating Lattice CPLD firmware");

    auto cpldManager = std::make_unique<CpldLatticeManager>(
        ctx, bus, address, hardwareCompatible, image, imageSize, chipname,
        "CFG0", false);

    co_return co_await cpldManager->updateFirmware(progressCallBack);
}

sdbusplus::async::task<bool> LatticeCPLD::getVersion(std::string& version)
{
    lg2::info("Getting Lattice CPLD version");
    auto versionPtr = std::make_unique<std::string>(version);
    auto cpldManager = std::make_unique<CpldLatticeManager>(
        ctx, bus, address, hardwareCompatible, nullptr, 0, chipname, "CFG0",
        false);

    if (versionPtr == nullptr)
    {
        lg2::error("Failed to allocate memory for version string.");
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
    for (const auto& type : supportedTypes)
    {
        CPLDFactory::instance().registerCPLD(
            type, [](sdbusplus::async::context& ctx,
                     const std::string& chipname, uint16_t bus, uint8_t address,
                     const std::string& hardwareCompatible) {
                // Create and return a LatticeCPLD instance
                // Pass the parameters to the constructor
                return std::make_unique<LatticeCPLD>(
                    ctx, chipname, bus, address, hardwareCompatible);
            });
    }
    return true;
}();

} // namespace
