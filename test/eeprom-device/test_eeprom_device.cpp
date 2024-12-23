#include "eeprom-device/eeprom_device.hpp"
#include "eeprom-device/eeprom_device_code_updater.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/async.hpp>

#include <iostream>

#include "gtest/gtest.h"

using namespace phosphor::software::eeprom;

// NOLINTNEXTLINE(readability-static-accessed-through-instance)
sdbusplus::async::task<> start(sdbusplus::async::context& ctx,
                               EEPROMDeviceSoftwareManager& eepromdcu)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto* parent = &eepromdcu;
    int32_t vendorIANA = 0x0000a015;
    std::string compatible = "com.testvendor.testcomponent";
    std::vector<std::string> gpioLines;
    std::vector<int> gpioPolarities;

    std::string objPath =
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleDevice";

    SoftwareConfig config(objPath, vendorIANA, compatible,
                          "EEPROMDeviceFirmware", "MB_Retimer");

    std::unique_ptr<DeviceVersion> deviceVersion =
        getVersionProvider("pt5161l", 0x01, 0x20);

    auto eepromDevice = std::make_unique<EEPROMDevice>(
        ctx, 0x01, 0x50, "at24", gpioLines, gpioPolarities,
        std::move(deviceVersion), config, parent);

    eepromdcu.devices.insert({objPath, std::move(eepromDevice)});

    ctx.request_stop();

    co_return;
}

TEST(EEPROMDeviceSoftwareManager, Constructor)
{
    sdbusplus::async::context ctx;

    EEPROMDeviceSoftwareManager eepromdcu(ctx);

    try
    {
        ctx.spawn(start(ctx, eepromdcu));
        ctx.run();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        assert(false);
    }
}
