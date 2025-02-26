#include "cpld.hpp"

namespace phosphor::software::cpld
{

sdbusplus::async::task<bool> CPLDDevice::updateDevice(const uint8_t* image,
                                                      size_t image_size)
{
    if (cpldInterface == nullptr)
    {
        lg2::error("CPLD interface is not initialized");
        co_return false;
    }
    else
    {
        setUpdateProgress(1);
        if (!(co_await cpldInterface->updateFirmware(
                false, image, image_size, [this](int percent) -> bool {
                    return this->setUpdateProgress(percent);
                })))
        {
            lg2::error("Failed to update CPLD firmware");
            co_return false;
        }

        setUpdateProgress(100);
        lg2::info("Successfully updated CPLD");
        co_return true;
    }
}

} // namespace phosphor::software::cpld
