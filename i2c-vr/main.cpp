#include "i2c_vr_device_code_updater.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>

using namespace std;

const std::vector<std::string> emConfigNames = {"XDPE1X2XXFW"};

// NOLINTNEXTLINE(readability-static-accessed-through-instance)
sdbusplus::async::task<> start(I2CVRSoftwareManager& i2cdcu)
{
    std::vector<std::string> configIntfs(emConfigNames.size());
    for (auto & name : emConfigNames)
    {
        configIntfs.push_back("xyz.openbmc_project.Configuration." + name);
    }
    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch)
    co_await i2cdcu.initDevices(configIntfs);

    co_return;
}

int main()
{
    sdbusplus::async::context ctx;

    I2CVRSoftwareManager i2cdcu(ctx);

    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch)
    ctx.spawn(start(i2cdcu));
    ctx.run();

    return 0;
}
