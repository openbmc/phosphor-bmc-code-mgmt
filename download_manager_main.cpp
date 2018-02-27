#include <sdbusplus/bus.hpp>
#include "config.h"
#include "download_manager.hpp"

int main(int argc, char* argv[])
{
    auto bus = sdbusplus::bus::new_default();

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager::manager objManager(bus, SOFTWARE_OBJPATH);

    phosphor::software::manager::Download manager(bus, SOFTWARE_OBJPATH);

    bus.request_name(DOWNLOAD_BUSNAME);

    while (true)
    {
        bus.process_discard();
        bus.wait();
    }
    return 0;
}
