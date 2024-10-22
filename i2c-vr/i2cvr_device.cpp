#include "i2cvr_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>

using namespace phosphor::software;
using namespace phosphor::software::manager;

namespace phosphor::software::i2c_vr::device{

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> I2CVRDevice::updateDevice(
    const uint8_t* image, size_t image_size)
// NOLINTEND(readability-static-accessed-through-instance)
{
    // Update loop, contition is size of image and reduced by size written to
    // chip.
    for (int i = 1; i <= 10; i++)
    {
            // acticationProgress->progress(1...100);
            (void)image;
            (void)image_size;
            setUpdateProgress(i*10);
            usleep(500000);
            lg2::info("[I2CVR] dry run, not writing to voltage regulator");
    }

    lg2::info("Update of VR done. Exiting...");
    co_return true;
}
}
