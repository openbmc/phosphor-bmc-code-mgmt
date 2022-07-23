#include "config.h"

#include "item_updater.hpp"

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>

boost::asio::io_context& getIOContext()
{
    static boost::asio::io_context io;
    return io;
}

int main()
{
    sdbusplus::asio::connection bus(getIOContext());

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager_t objManager(bus, SOFTWARE_OBJPATH);

    phosphor::software::updater::ItemUpdater updater(bus, SOFTWARE_OBJPATH);

    bus.request_name(BUSNAME_UPDATER);

    getIOContext().run();

    return 0;
}
