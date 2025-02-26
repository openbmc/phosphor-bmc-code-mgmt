#include "cpld_code_updater.hpp"

#include <sdbusplus/async.hpp>

using namespace phosphor::software::cpld;

// NOLINTNEXTLINE(readability-static-accessed-through-instance)
sdbusplus::async::task<> start(CPLDCodeUpdater& cpldcu)
{
    std::vector<std::string> configIntfs = {
        "xyz.openbmc_project.Configuration." + configTypeCPLD};

    co_await cpldcu.initDevices(configIntfs);

    co_return;
}

int main()
{
    // NOLINTBEGIN
    sdbusplus::async::context ctx;

    CPLDCodeUpdater cpldcu(ctx);

    ctx.spawn(start(cpldcu));

    ctx.run();

    // NOLINTEND
    return 0;
}
