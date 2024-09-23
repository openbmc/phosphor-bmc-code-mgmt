#include "spi_device_code_updater.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

// implementing the design
// https://github.com/openbmc/docs/blob/377ed14df4913010752ee2faff994a50e12a6316/designs/code-update.md

int main()
{
    boost::asio::io_context io;
    bool dryRun = true;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    SPIDeviceCodeUpdater spidcu(systemBus, io, dryRun);

    io.run();
    return 0;
}

