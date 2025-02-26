#include "cpld.hpp"

namespace phosphor::software::cpld::device
{

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CPLDDevice::updateDevice(const uint8_t* image,
                                                      size_t image_size)
// NOLINTEND(readability-static-accessed-through-instance)
{
    if (cpldInterface == nullptr)
    {
        lg2::error("CPLD interface is not initialized");
        co_return false;
    }
    else
    {
        setUpdateProgress(1);
        if (!cpldInterface->updateFirmware(false, image, image_size))
        {
            lg2::error("Failed to update CPLD firmware");
            co_return false;
        }
        else
        {
            setUpdateProgress(100);
            lg2::info("Successfully updated CPLD");
            co_return true;
        }
    }
}

} // namespace phosphor::software::cpld::device
