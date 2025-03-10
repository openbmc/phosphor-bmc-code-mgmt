#include "dummyDevice.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

DummyDevice::DummyDevice(sdbusplus::async::context& ctx) : VoltageRegulator(ctx)
{
    info("DummyDevice Voltage Regulator created");
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> DummyDevice::verifyImage(const uint8_t* image,
                                                      size_t imageSize)
// NOLINTEND(readability-static-accessed-through-instance)
{
    (void)*image;
    (void)imageSize;
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::microseconds(5000));
    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> DummyDevice::updateFirmware(bool force)
// NOLINTEND(readability-static-accessed-through-instance)
{
    if (force)
    {
        info("DummyDevice Update will be forced!");
    }

    co_await sdbusplus::async::sleep_for(ctx, std::chrono::microseconds(5000));
    info("DummyDevice Update done");

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> DummyDevice::reset()
// NOLINTEND(readability-static-accessed-through-instance)
{
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::microseconds(5000));
    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> DummyDevice::getCRC(uint32_t* checksum)
// NOLINTEND(readability-static-accessed-through-instance)
{
    *checksum = 0x0000BEEF;
    co_return true;
}

bool DummyDevice::forcedUpdateAllowed()
{
    return true;
}

} // namespace phosphor::software::VR
