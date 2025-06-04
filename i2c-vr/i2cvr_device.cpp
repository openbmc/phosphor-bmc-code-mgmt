#include "i2cvr_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>

namespace phosphor::software::i2c_vr::device
{

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> I2CVRDevice::updateDevice(const uint8_t* image,
                                                       size_t imageSize)
// NOLINTEND(readability-static-accessed-through-instance)
{
    bool ret = false;
    setUpdateProgress(20);

    if (!vrInterface)
    {
        co_return false;
    }

    ret = vrInterface->verifyImage(image, imageSize);
    if (!ret)
    {
        co_return false;
    }

    setUpdateProgress(50);

    ret = co_await vrInterface->updateFirmware(false);
    if (!ret)
    {
        co_return false;
    }

    setUpdateProgress(80);

    ret = co_await vrInterface->reset();
    if (!ret)
    {
        co_return false;
    }

    setUpdateProgress(100);

    lg2::info("Successfully updated VR {NAME}", "NAME", config.configName);

    co_return true;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> I2CVRDevice::getVersion(uint32_t* sum) const
// NOLINTEND(readability-static-accessed-through-instance)
{
    if (!(co_await this->vrInterface->getCRC(this->vrInterface->i2cInterface,
                                             sum)))
    {
        co_return false;
    }
    co_return true;
}

} // namespace phosphor::software::i2c_vr::device
