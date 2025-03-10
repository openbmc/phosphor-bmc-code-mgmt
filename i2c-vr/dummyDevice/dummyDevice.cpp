#include "dummyDevice.hpp"

#include <sdbusplus/async.hpp>
#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace SDBusAsync = sdbusplus::async;

namespace phosphor::software::VR
{

DummyDevice::DummyDevice(SDBusAsync::context& ctx) : VoltageRegulator(ctx)
{
    info("DummyDevice Voltage Regulator created");
}

SDBusAsync::task<bool> DummyDevice::verifyImage(const uint8_t* image, size_t imageSize)
{
    (void)*image;
    (void)imageSize;
    SDBusAsync::sleep_for(ctx, std::chrono::microseconds(5000));
    co_return true;
}

SDBusAsync::task<bool> DummyDevice::updateFirmware(bool force)
{
    if (force)
    {
       info("DummyDevice Update will be forced!");
    }

    SDBusAsync::sleep_for(ctx, std::chrono::microseconds(5000));
    info("DummyDevice Update done");

    co_return true;
}

SDBusAsync::task<bool> DummyDevice::reset()
{
    SDBusAsync::sleep_for(ctx, std::chrono::microseconds(5000));
    co_return true;
}

bool DummyDevice::getCRC(uint32_t* checksum)
{
    *checksum = 0x0000BEEF;
    return true;
}

}

