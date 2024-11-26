#include "bios_software_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>

void run(bool dryRun)
{
    sdbusplus::async::context ctx;

    std::vector<std::string> configIntfs = {
        "xyz.openbmc_project.Configuration.SPIFlash",
    };

    BIOSSoftwareManager spidcu(ctx, dryRun);

    ctx.spawn(spidcu.initDevices(configIntfs));

    ctx.run();
}

int main(int argc, char* argv[])
{
    bool dryRun = false;

    for (int i = 1; i < argc; i++)
    {
        std::string arg = std::string(argv[i]);
        if (arg == "--dryrun")
        {
            dryRun = true;
        }
    }

    run(dryRun);

    return 0;
}
