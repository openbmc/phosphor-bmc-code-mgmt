#include "config.h"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

PHOSPHOR_LOG2_USING;

int main()
{
    info("Creating Software Manager");

    auto path = std::string(SOFTWARE_OBJPATH) + "/bmc";
    sdbusplus::async::context ctx;
    sdbusplus::server::manager_t manager{ctx, path.c_str()};

    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    ctx.spawn([=](sdbusplus::async::context& ctx) -> sdbusplus::async::task<> {
        constexpr auto serviceName = "xyz.openbmc_project.Software.Manager";
        ctx.request_name(serviceName);
        co_return;
    }(ctx));

    ctx.run();

    return 0;
}
