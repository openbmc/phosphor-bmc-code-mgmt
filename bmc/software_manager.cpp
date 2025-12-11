#include "config.h"

#include "item_updater.hpp"
#include "update_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

#include <string>

using ItemUpdaterIntf = phosphor::software::updater::ItemUpdater;
using UpdateManager = phosphor::software::update::Manager;

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

    // Create the Software.Update interface at the BMC path
    // When BMC_MULTIPART_UPDATE is enabled, this also exposes MultipartUpdate
    UpdateManager bmcUpdateManager{ctx, bmcPath, bmcItemUpdater};

#ifdef HOST_BIOS_UPGRADE
    ItemUpdaterIntf biosItemUpdater{ctx, biosPath,
                                    ItemUpdaterIntf::UpdaterType::BIOS};
#endif // HOST_BIOS_UPGRADE
    ctx.request_name(serviceName);

    ctx.run();
    return 0;
}
