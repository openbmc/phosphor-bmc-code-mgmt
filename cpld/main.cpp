#include "cpld_software_manager.hpp"

#include <sdbusplus/async.hpp>

using namespace phosphor::software::cpld;

// NOLINTNEXTLINE(readability-static-accessed-through-instance)
sdbusplus::async::task<> start(CPLDSoftwareManager& cpldcu)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::vector<std::string> configIntfs = {
        "xyz.openbmc_project.Configuration." + configTypeCPLD};

    co_await cpldcu.initDevices(configIntfs);

    co_return;
}

int main()
{
    sdbusplus::async::context ctx;

    CPLDSoftwareManager cpldcu(ctx);

    ctx.spawn(start(cpldcu));

    ctx.run();

    return 0;
}
