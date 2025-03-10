#include "dummyDevice.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

PHOSPHOR_LOG2_USING;

namespace SDBusAsync = sdbusplus::async;

namespace phosphor::software::VR
{

DummyDevice::DummyDevice(SDBusAsync::context& ctx) : VoltageRegulator(ctx)
{
    info("DummyDevice Voltage Regulator created");
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
SDBusAsync::task<bool> DummyDevice::verifyImage(const uint8_t* image,
                                                size_t imageSize)
// NOLINTEND(readability-static-accessed-through-instance)
{
    (void)*image;
    (void)imageSize;
    SDBusAsync::sleep_for(ctx, std::chrono::microseconds(5000));
    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
SDBusAsync::task<bool> DummyDevice::updateFirmware(bool force)
// NOLINTEND(readability-static-accessed-through-instance)
{
    if (force)
    {
        info("DummyDevice Update will be forced!");
    }

    SDBusAsync::sleep_for(ctx, std::chrono::microseconds(5000));
    info("DummyDevice Update done");

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
SDBusAsync::task<bool> DummyDevice::reset()
// NOLINTEND(readability-static-accessed-through-instance)
{
    SDBusAsync::sleep_for(ctx, std::chrono::microseconds(5000));
    co_return true;
}

bool DummyDevice::getCRC(uint32_t* checksum)
{
    *checksum = 0x0000BEEF;
    return true;
}

} // namespace phosphor::software::VR
