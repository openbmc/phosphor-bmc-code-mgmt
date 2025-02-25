#include "bios-spi/bios_software_manager.hpp"
#include "bios-spi/spi_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <iostream>
#include <memory>

int main()
{
    sdbusplus::async::context ctx;

    try
    {
        BIOSSoftwareManager spidcu(ctx, true);
        uint32_t vendorIANA = 0x0000a015;
        std::string compatible = "com.testvendor.testcomponent";
        auto* cu = &spidcu;
        std::vector<std::string> gpioNames;
        std::vector<uint64_t> gpioValues;

        std::string objPath =
            "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleDevice";

        SoftwareConfig config(objPath, vendorIANA, compatible, configTypeBIOS,
                              "HostSPI");

        auto sd = std::make_unique<SPIDevice>(
            ctx, 1, 0, true, true, gpioNames, gpioValues, config, cu,
            FLASH_LAYOUT_FLAT, FLASH_TOOL_NONE);

        spidcu.devices.insert({objPath, std::move(sd)});
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
