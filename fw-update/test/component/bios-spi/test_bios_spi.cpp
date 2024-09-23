#include "spi_device_code_updater.hpp"
#include "spi_device.hpp"

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

    try {
        SPIDeviceCodeUpdater spidcu(io, true);
        std::string vendorIANA = "";
        std::string compatible = "";
        SPIDeviceCodeUpdater* cu = &spidcu;
        std::vector<std::string> gpioNames;
        std::vector<uint8_t> gpioValues;

        auto sd = std::make_shared<SPIDevice>(io, true, true, gpioNames, gpioValues, vendorIANA, compatible, cu);

        spidcu.devices.insert(sd);

    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
