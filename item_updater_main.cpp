#include "config.h"

#include "item_updater.hpp"
#include "software_utils.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>

using ItemUpdaterIntf = phosphor::software::updater::ItemUpdater;

int main()
{
    sdbusplus::async::context ctx;

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager_t objManager(ctx, SOFTWARE_OBJPATH);

    ItemUpdaterIntf updater(ctx, SOFTWARE_OBJPATH,
                            ItemUpdaterIntf::UpdaterType::ALL, false);

    ctx.request_name(BUSNAME_UPDATER);

    ctx.run();

    return 0;
}
