#include "i2c_vr_device_code_updater.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>

using namespace std;

// NOLINTNEXTLINE(readability-static-accessed-through-instance)
sdbusplus::async::task<> start(I2CVRDeviceCodeUpdater& i2cdcu)
{
    std::vector<std::string> configIntfs = {
        "xyz.openbmc_project.Configuration." + configTypeVRDevice};
    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch)
    co_await i2cdcu.initDevices(configIntfs);

    co_return;
}

void run(bool dryrun)
{
    sdbusplus::async::context ctx;

    I2CVRDeviceCodeUpdater i2cdcu(ctx, dryrun);

    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch)
    ctx.spawn(start(i2cdcu));
    ctx.run();
}

int main(int argc, char* argv[])
{
    bool dryRun = false;
    for (int i = 1; i < argc; i++)
    {
        std::string arg = std::string(argv[i]);
        {
            dryRun = true;
        }
    }

    run(dryRun);

    return 0;
}
