#include "interface.hpp"

#include "lattice.hpp"

#include <phosphor-logging/lg2.hpp>

namespace phosphor::software::cpld
{

const std::vector<std::string> supportedTypes = {
    "LatticeXO2Firmware",
    "LatticeXO3Firmware",
};

sdbusplus::async::task<bool> LatticeCPLD::updateFirmware(
    bool /*force*/, const uint8_t* image, size_t imageSize,
    std::function<bool(int)> progressCallBack)
{
    lg2::info("Updating Lattice CPLD firmware");

    std::replace(chipname.begin(), chipname.end(), '_', '-');
    auto cpldManager = std::make_unique<CpldLatticeManager>(
        ctx, bus, address, image, imageSize, chipname, "CFG0", false,
        progressCallBack);

    co_return co_await cpldManager->updateFirmware();
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
            type,
            [](sdbusplus::async::context& ctx, const std::string& chipname,
               uint16_t bus, uint8_t address) {
                // Create and return a LatticeCPLD instance
                // Pass the parameters to the constructor
                return std::make_unique<LatticeCPLD>(ctx, chipname, bus,
                                                     address);
            });
    }
    return true;
}();

} // namespace
