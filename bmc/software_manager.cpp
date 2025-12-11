#include "config.h"

#include "item_updater.hpp"
#include "update_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

#include <string>
#include <type_traits>

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

    // Create the Software.Update interface at the BMC path
    // When multipart update is enabled, use MultipartUpdateManager which
    // exposes both Update and MultipartUpdate interfaces
    using namespace phosphor::software::update;
    using BmcUpdateManagerType =
        std::conditional_t<bmcMultipartUpdateEnabled, MultipartUpdateManager,
                           UpdateManager>;
    BmcUpdateManagerType bmcUpdateManager{ctx, bmcPath, bmcItemUpdater};

#ifdef HOST_BIOS_UPGRADE
    ItemUpdaterIntf biosItemUpdater{ctx, biosPath,
                                    ItemUpdaterIntf::UpdaterType::BIOS};
#endif // HOST_BIOS_UPGRADE
    ctx.request_name(serviceName);

    ctx.run();
    return 0;
}
