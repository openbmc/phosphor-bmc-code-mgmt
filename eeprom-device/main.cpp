#include "eeprom_device_code_updater.hpp"

#include <sdbusplus/async.hpp>

using namespace phosphor::software::eeprom;

// NOLINTNEXTLINE(readability-static-accessed-through-instance)
sdbusplus::async::task<> start(EEPROMDeviceCodeUpdater& eepromdcu)
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

    EEPROMDeviceCodeUpdater eepromdcu(ctx);

    ctx.spawn(start(eepromdcu));

    ctx.run();

    // NOLINTEND
    return 0;
}
