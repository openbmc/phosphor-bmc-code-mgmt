#include "eeprom_device_software_manager.hpp"

#include <sdbusplus/async.hpp>

int main()
{
    sdbusplus::async::context ctx;

    std::vector<std::string> configIntfs = {
        "xyz.openbmc_project.Configuration." + configTypeEEPROMDevice};

    EEPROMDeviceSoftwareManager eepromdevicecu(ctx);

    ctx.spawn(eepromdevicecu.initDevices(configIntfs));

    ctx.run();

    return 0;
}
