#include "i2cvr_software_manager.hpp"

#include <sdbusplus/async.hpp>

namespace SDBusAsync = sdbusplus::async;

int main()
{
    SDBusAsync::context ctx;

    I2CVRSoftwareManager i2cVRSoftwareManager(ctx);

    i2cVRSoftwareManager.start();
    return 0;
}
