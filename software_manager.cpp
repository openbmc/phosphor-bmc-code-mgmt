#include "item_updater.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

using ItemUpdaterIntf = phosphor::software::updater::ItemUpdater;

PHOSPHOR_LOG2_USING;

int main()
{
    info("Creating Software Manager");
    auto path = std::string(SOFTWARE_OBJPATH) + "/bmc";
    sdbusplus::async::context ctx;
    sdbusplus::server::manager_t manager{ctx, path.c_str()};

    constexpr auto serviceName = "xyz.openbmc_project.Software.Manager";

    ItemUpdaterIntf itemUpdater{ctx, path};
    ctx.request_name(serviceName);

    ctx.run();
    return 0;
}
