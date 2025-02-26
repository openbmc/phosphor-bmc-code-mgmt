#include "interface.hpp"

#include "lattice.hpp"

#include <phosphor-logging/lg2.hpp>

namespace phosphor::software::cpld
{

bool LatticeCPLD::updateFirmware(bool /*force*/, const uint8_t* image,
                                 size_t imageSize)
{
    // Implement the firmware update logic for Lattice CPLD here
    // This is a placeholder implementation
    lg2::info("Updating Lattice CPLD firmware");

    auto cpldManager = CpldLatticeManager(bus, address, image, imageSize,
                                          chipname, "i2c", "CFG0", false);

    return static_cast<bool>(cpldManager.fwUpdate() >= 0);
}

} // namespace phosphor::software::cpld

// Factory function to create lattice CPLD device
namespace
{
using namespace phosphor::software::cpld;
const bool registered = [] {
    CPLDFactory::instance().registerVendor(
        "lattice",
        [](const std::string& chipname, uint16_t bus, uint8_t address) {
            // Create and return a LatticeCPLD instance
            // Pass the parameters to the constructor
            return std::make_unique<LatticeCPLD>(chipname, bus, address);
        });
    return true;
}();
} // namespace

