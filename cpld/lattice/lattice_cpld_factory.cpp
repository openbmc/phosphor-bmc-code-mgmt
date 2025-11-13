#include "lattice_cpld_factory.hpp"

#include "lattice_xo3_cpld.hpp"
#include "lattice_xo5_cpld.hpp"

#include <phosphor-logging/lg2.hpp>

namespace phosphor::software::cpld
{

std::unique_ptr<LatticeBaseCPLD> LatticeCPLDFactory::getLatticeCPLD()
{
    if (supportedDeviceMap.find(chipEnum) == supportedDeviceMap.end())
    {
        // invalid
        lg2::error("Unsupported Lattice CPLD chip enum: {CHIPENUM}", "CHIPENUM",
                   chipEnum);
        return nullptr;
    }

    auto chipFamily = supportedDeviceMap.at(chipEnum).chipFamily;
    auto chipModelStr =
        getLatticeChipStr(chipEnum, latticeStringType::modelString);
    switch (chipFamily)
    {
        case latticeChipFamily::XO2:
        case latticeChipFamily::XO3:
            return std::make_unique<LatticeXO3CPLD>(
                CPLDInterface::ctx, CPLDInterface::bus, CPLDInterface::address,
                chipModelStr, "CFG0", false);
        case latticeChipFamily::XO5:
            return std::make_unique<LatticeXO5CPLD>(
                CPLDInterface::ctx, CPLDInterface::bus, CPLDInterface::address,
                chipModelStr, "CFG0", false);
        default:
            lg2::error("Unsupported Lattice CPLD chip family: {CHIPMODEL}",
                       "CHIPMODEL", chipModelStr);
            return nullptr;
    }
}

std::unique_ptr<LatticeBaseCPLDJtag> LatticeCPLDFactory::getLatticeCPLDJtag()
{
    if (supportedDeviceMap.find(chipEnum) == supportedDeviceMap.end())
    {
        lg2::error("Unsupported Lattice CPLD chip enum: {CHIPENUM}", "CHIPENUM",
                   chipEnum);
        return nullptr;
    }

    auto chipFamily = supportedDeviceMap.at(chipEnum).chipFamily;
    auto chipModelStr =
        getLatticeChipStr(chipEnum, latticeStringType::modelString);
    switch (chipFamily)
    {
        case latticeChipFamily::XO2:
        case latticeChipFamily::XO3:
            return std::make_unique<LatticeXO3CPLDJtag>(
                ctx, bus, address, jtagIndex, chipname, chipModelStr, gpioLines,
                gpioValues);
        case latticeChipFamily::XO5:
            return std::make_unique<LatticeXO5CPLDJtag>(
                ctx, bus, address, jtagIndex, chipname, chipModelStr, gpioLines,
                gpioValues);
        default:
            lg2::error("Unsupported Lattice CPLD chip family: {CHIPMODEL}",
                       "CHIPMODEL", chipModelStr);
            return nullptr;
    }
}

sdbusplus::async::task<bool> LatticeCPLDFactory::updateFirmware(
    bool /*force*/, const uint8_t* image, size_t imageSize,
    std::function<bool(int)> progressCallBack)
{
    lg2::info("Updating Lattice CPLD firmware");

    if (protocol == "JTAG")
    {
        auto cpldManager = getLatticeCPLDJtag();
        if (cpldManager == nullptr)
        {
            lg2::error("CPLD manager is not initialized.");
            co_return false;
        }
        co_return co_await cpldManager->updateFirmware(image, imageSize,
                                                       progressCallBack);
    }
    else
    {
        auto cpldManager = getLatticeCPLD();
        if (cpldManager == nullptr)
        {
            lg2::error("CPLD manager is not initialized.");
            co_return false;
        }
        co_return co_await cpldManager->updateFirmware(image, imageSize,
                                                       progressCallBack);
    }
}

sdbusplus::async::task<bool> LatticeCPLDFactory::getVersion(
    std::string& version)
{
    lg2::info("Getting Lattice CPLD version");

    if (protocol == "JTAG")
    {
        auto cpldManager = getLatticeCPLDJtag();
        if (cpldManager == nullptr)
        {
            lg2::error("CPLD manager is not initialized.");
            co_return false;
        }
        co_return co_await cpldManager->getVersion(version);
    }
    else
    {
        auto cpldManager = getLatticeCPLD();
        if (cpldManager == nullptr)
        {
            lg2::error("CPLD manager is not initialized.");
            co_return false;
        }
        co_return co_await cpldManager->getVersion(version);
    }
}

} // namespace phosphor::software::cpld

// Factory function to create lattice CPLD device
namespace
{
using namespace phosphor::software::cpld;

// Register all the CPLD type with the CPLD factory
const bool vendorRegistered = [] {
    for (const auto& [chipEnum, info] : supportedDeviceMap)
    {
        auto typeStr =
            getLatticeChipStr(chipEnum, latticeStringType::typeString);
        CPLDFactory::instance().registerCPLD(
            typeStr,
            [chipEnum](sdbusplus::async::context& ctx,
                       const std::string& chipName, const std::string& protocol,
                       const std::string& jtagIndex, uint16_t bus,
                       uint8_t address,
                       const std::vector<std::string>& gpioLines,
                       const std::vector<std::string>& gpioValues) {
                // Create and return a LatticeCPLD instance
                // Pass the parameters to the constructor
                return std::make_unique<LatticeCPLDFactory>(
                    ctx, chipName, chipEnum, protocol, jtagIndex, bus, address,
                    gpioLines, gpioValues);
            });
    }
    return true;
}();

} // namespace
