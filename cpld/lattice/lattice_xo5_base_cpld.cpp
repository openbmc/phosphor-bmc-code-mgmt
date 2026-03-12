#include "lattice_xo5_base_cpld.hpp"

#include <phosphor-logging/lg2.hpp>

namespace phosphor::software::cpld
{

LatticeXO5BaseCPLD::LatticeXO5BaseCPLD(
    sdbusplus::async::context& ctx, const uint16_t bus, const uint8_t address,
    const std::string& chip, const std::string& target,
    std::chrono::milliseconds interval, const bool debugMode) :
    LatticeBaseCPLD(ctx, bus, address, chip, target, debugMode),
    pollInterval(interval)
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

sdbusplus::async::task<bool> LatticeXO5BaseCPLD::waitUntilReady(
    std::chrono::milliseconds timeout)
{
    const auto endTime = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < endTime)
    {
        if (co_await checkDeviceReady())
        {
            co_return true;
        }

        co_await sdbusplus::async::sleep_for(ctx, pollInterval);
    }

    lg2::error("Timeout waiting for device ready");
    co_return false;
}

uint32_t LatticeXO5BaseCPLD::extractUint32(const std::vector<uint8_t>& data,
                                           size_t offset, bool isBigEndian)
{
    if (data.size() < offset + 4)
    {
        return 0;
    }

    if (isBigEndian)
    {
        return (data[offset] << 24) | (data[offset + 1] << 16) |
               (data[offset + 2] << 8) | (data[offset + 3]);
    }
    else
    {
        return (data[offset + 3] << 24) | (data[offset + 2] << 16) |
               (data[offset + 1] << 8) | (data[offset]);
    }
}
} // namespace phosphor::software::cpld
