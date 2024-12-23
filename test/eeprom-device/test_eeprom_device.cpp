#include "eeprom-device/eeprom_device.hpp"
#include "eeprom-device/eeprom_device_code_updater.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/async.hpp>

#include <iostream>

using namespace phosphor::software::eeprom;
using namespace phosphor::software::eeprom::device;
using namespace phosphor::software::eeprom::version;

int main()
{
    sdbusplus::async::context ctx;

    try
    {
        EEPROMDeviceCodeUpdater eepromdcu(ctx);
        EEPROMDeviceCodeUpdater* parent = &eepromdcu;

        uint32_t vendorIANA = 0x0000a015;
        std::string compatible = "com.testvendor.testcomponent";
        std::vector<std::string> gpioLines;
        std::vector<uint8_t> gpioPolarities;

        std::string objPath =
            "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleDevice";

        SoftwareConfig config(objPath, vendorIANA, compatible,
                              "EEPROMDeviceFirmware", "MB_Retimer");

        std::unique_ptr<VersionProvider> versionProvider =
            getVersionProvider("pt5161l", 0x01, 0x20);

        auto eepromDevice = std::make_unique<EEPROMDevice>(
            ctx, 0x01, 0x50, "at24", gpioLines, gpioPolarities,
            std::move(versionProvider), config, parent);

        eepromdcu.devices.insert({objPath, std::move(eepromDevice)});
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
