#include "item_updater.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

using ItemUpdaterIntf = phosphor::software::updater::ItemUpdater;

PHOSPHOR_LOG2_USING;

int main()
{
    info("Creating Software Manager");
    auto bmcPath = std::string(SOFTWARE_OBJPATH) + "/bmc";
    auto biosPath = std::string(SOFTWARE_OBJPATH) + "/bios";
    sdbusplus::async::context ctx;
    sdbusplus::server::manager_t manager{ctx, SOFTWARE_OBJPATH};

    constexpr auto serviceName = "xyz.openbmc_project.Software.Manager";

    ItemUpdaterIntf bmcItemUpdater{ctx, bmcPath,
                                   ItemUpdaterIntf::UpdaterType::BMC};
    ItemUpdaterIntf biosItemUpdater{ctx, biosPath,
                                    ItemUpdaterIntf::UpdaterType::BIOS};
    ctx.request_name(serviceName);

    ctx.run();
    return 0;
}
