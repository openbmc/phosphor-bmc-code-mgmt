#include "config.h"

#include "item_updater.hpp"

#include <boost/asio.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>

boost::asio::io_service& getIOService()
{
    static boost::asio::io_service io;
    return io;
}

int main()
{
    sdbusplus::asio::connection bus(getIOService());

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager::manager objManager(bus, SOFTWARE_OBJPATH);

    phosphor::software::updater::ItemUpdater updater(bus, SOFTWARE_OBJPATH);

    bus.request_name(BUSNAME_UPDATER);

    getIOService().run();

    return 0;
}
