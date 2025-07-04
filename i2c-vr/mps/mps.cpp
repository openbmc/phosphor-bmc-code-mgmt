#include "mps.hpp"

#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

sdbusplus::async::task<bool> MPSVoltageRegulator::writeData(
    const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> rbuf;
    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    if (!i2cInterface.sendReceive(data, rbuf))
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    {
        error("Failed to write data {DATA}", "DATA", lg2::hex,
              bytesToInt<uint32_t>(data, true));
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MPSVoltageRegulator::setPage(MPSPage page)
{
    if (!co_await writeData(PMBusCmd::Page, page))
    {
        error("Failed to set page {PAGE}", "PAGE", static_cast<uint8_t>(page));
        co_return false;
    }

    co_return true;
}

} // namespace phosphor::software::VR
