#include "i2c_vr_device_code_updater.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

int main()
{
    sdbusplus::async::context io;
    I2CVRDeviceCodeUpdater i2cdcu(io, true);

    io.spawn(i2cdcu.getInitialConfiguration());

    io.run();
    return 0;
}
