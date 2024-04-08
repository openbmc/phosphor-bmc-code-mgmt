#include "config.h"

#include "item_updater.hpp"
#include "software_utils.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>

int main()
{
    sdbusplus::async::context ctx;

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager_t objManager(ctx, SOFTWARE_OBJPATH);

    phosphor::software::updater::ItemUpdater updater(ctx, SOFTWARE_OBJPATH,
                                                     false);

    ctx.request_name(BUSNAME_UPDATER);

    ctx.run();

    return 0;
}
