#include "spi_device_code_updater.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>

// NOLINTNEXTLINE(readability-static-accessed-through-instance)
sdbusplus::async::task<> start(SPIDeviceCodeUpdater& spidcu)
{
    std::vector<std::string> configIntfs = {
        "xyz.openbmc_project.Configuration." + configTypeBIOS};

    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch)
    co_await spidcu.initDevices(configIntfs);

    co_return;
}

void run(bool dryRun)
{
    sdbusplus::async::context ctx;

    SPIDeviceCodeUpdater spidcu(ctx, dryRun);

    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch)
    ctx.spawn(start(spidcu));

    ctx.run();
}

int main(int argc, char* argv[])
{
    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
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

    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
}
