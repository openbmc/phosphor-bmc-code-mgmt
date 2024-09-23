#include "spi_device.hpp"
#include "spi_device_code_updater.hpp"

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
    sdbusplus::async::context io;

    try
    {
        SPIDeviceCodeUpdater spidcu(io, true, false);
        uint32_t vendorIANA = 0x0000a015;
        std::string compatible = "";
        SPIDeviceCodeUpdater* cu = &spidcu;
        std::vector<std::string> gpioNames;
        std::vector<uint8_t> gpioValues;

        std::string objPathConfig = "/dummy/config/objectPath";

        auto sd = std::make_shared<SPIDevice>(
            io, "1e630000.spi", true, true, gpioNames, gpioValues, vendorIANA,
            compatible, cu, objPathConfig.c_str(), true, true);

        spidcu.devices.insert(sd);
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
