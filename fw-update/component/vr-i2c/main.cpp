#include "i2c_vr_device_code_updater.hpp"

#include <sdbusplus/async.hpp>

int main()
{
    sdbusplus::async::context io;
    I2CVRDeviceCodeUpdater i2cdcu(io, true);

    io.spawn(i2cdcu.getInitialConfiguration());

    io.run();
    return 0;
}
