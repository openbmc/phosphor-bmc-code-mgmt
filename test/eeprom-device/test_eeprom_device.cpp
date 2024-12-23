#include "eeprom-device/eeprom_device.hpp"
#include "eeprom-device/eeprom_device_code_updater.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/async.hpp>

#include <iostream>

using namespace phosphor::software::eeprom;
using namespace phosphor::software::eeprom::device;

int main()
{
    sdbusplus::async::context ctx;

    try
    {
        EEPROMDeviceCodeUpdater eepromdcu(ctx);
        EEPROMDeviceCodeUpdater* parent = &eepromdcu;

        uint32_t vendorIANA = 0x0000a015;
        std::string compatible = "com.testvendor.testcomponent";
        uint8_t bus = 0x01;
        uint8_t addr = 0x50;
        std::string chipModel = "at24";
        std::vector<std::string> gpioLines;
        std::vector<uint8_t> gpioPolarities;

        std::string objPath =
            "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleDevice";

        SoftwareConfig config(objPath, vendorIANA, compatible,
                              "EEPROMDeviceFirmware", "MB_Retimer");

        auto eepromDevice =
            std::make_unique<EEPROMDevice>(ctx, bus, addr, chipModel, gpioLines,
                                           gpioPolarities, config, parent);

        eepromdcu.devices.insert({objPath, std::move(eepromDevice)});
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
