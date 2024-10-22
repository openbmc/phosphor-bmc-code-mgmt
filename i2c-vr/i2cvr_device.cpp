#include "i2cvr_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>

namespace phosphor::software::i2c_vr::device{

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> I2CVRDevice::updateDevice(
    const uint8_t* image, size_t imageSize)
// NOLINTEND(readability-static-accessed-through-instance)
{
    if (!( co_await vrInterface->verifyImage(image, imageSize)))
    {
        co_return false;
    }

    // setUpdateProgress unused. Working on a solution to pass it through to the update function.
    if (!( co_await vrInterface->updateFirmware(false)))
    {
        co_return false;
    }

    if (!(co_await vrInterface->reset()))
    {
        co_return false;
    }

    lg2::info("Successfully updated VR {NAME}", "NAME", config.configName);
    co_return true;
}

int I2CVRDevice::getVersion(uint32_t* sum)
{
    return this->vrInterface->getCRC(sum);
}

}
