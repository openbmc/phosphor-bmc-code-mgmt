#include "i2cvr_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>

namespace phosphor::software::i2c_vr::device{

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> I2CVRDevice::updateDevice(
    const uint8_t* image, size_t image_size)
// NOLINTEND(readability-static-accessed-through-instance)
{
    // setUpdateProgress unused. Working on a solution to pass it through to the update function.
    if (vrInterface->fwUpdate(image, image_size, false) < 0)
    {
        co_return false;
    }
    lg2::info("Update of VR '{NAME}' done.", "NAME", config.configName);
    co_return true;
}

int I2CVRDevice::getVersion(uint32_t* sum)
{
    return this->vrInterface->getCRC(sum);
}

}
