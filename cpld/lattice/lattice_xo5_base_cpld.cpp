#include "lattice_xo5_base_cpld.hpp"

#include <phosphor-logging/lg2.hpp>

namespace phosphor::software::cpld
{

LatticeXO5BaseCPLD::LatticeXO5BaseCPLD(
    sdbusplus::async::context& ctx, const uint16_t bus, const uint8_t address,
    const std::string& chip, const std::string& target, const bool debugMode) :
    LatticeBaseCPLD(ctx, bus, address, chip, target, debugMode)
{
    lg2::info("Lattice XO5 Base Driver initialized.");
}

sdbusplus::async::task<bool> LatticeXO5BaseCPLD::doUpdate()
{
    lg2::debug("Programming {TARGET}...", "TARGET", target);

    if (!(co_await programCfg()))
    {
        lg2::error("Program cfg data failed.");
        co_return false;
    }
    co_return true;
}

} // namespace phosphor::software::cpld
