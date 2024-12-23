#include "eeprom_device_code_updater.hpp"

#include <sdbusplus/async.hpp>

using EEPROMDeviceSoftwareManager =
    phosphor::software::eeprom::EEPROMDeviceSoftwareManager;

// NOLINTNEXTLINE(readability-static-accessed-through-instance)
sdbusplus::async::task<> start(EEPROMDeviceSoftwareManager& eepromdcu)
{
    std::vector<std::string> configIntfs = {
        "xyz.openbmc_project.Configuration." + configTypeEEPROMDevice};

    co_await eepromdcu.initDevices(configIntfs);

    co_return;
}

int main()
{
    // NOLINTBEGIN
    sdbusplus::async::context ctx;

    EEPROMDeviceSoftwareManager eepromdcu(ctx);

    ctx.spawn(start(eepromdcu));

    ctx.run();

    // NOLINTEND
    return 0;
}
