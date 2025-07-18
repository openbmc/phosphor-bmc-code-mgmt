#include "i2cvr_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>

namespace phosphor::software::i2c_vr::device
{

sdbusplus::async::task<bool> I2CVRDevice::updateDevice(const uint8_t* image,
                                                       size_t imageSize)
{
    setUpdateProgress(20);

    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    if (!(co_await vrInterface->verifyImage(image, imageSize)))
    //  NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    {
        co_return false;
    }

    setUpdateProgress(50);

    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    if (!(co_await vrInterface->updateFirmware(false)))
    //  NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    {
        co_return false;
    }

    setUpdateProgress(80);

    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    if (!(co_await vrInterface->reset()))
    //  NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    {
        co_return false;
    }

    setUpdateProgress(100);

    lg2::info("Successfully updated VR {NAME}", "NAME", config.configName);

    co_return true;
}

sdbusplus::async::task<bool> I2CVRDevice::getVersion(uint32_t* sum) const
{
    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    if (!(co_await this->vrInterface->getCRC(sum)))
    //  NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    {
        co_return false;
    }
    co_return true;
}

} // namespace phosphor::software::i2c_vr::device
