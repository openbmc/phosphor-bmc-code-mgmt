#include <sdbusplus/bus.hpp>
#include "config.h"
#include "delete_manager.hpp"

int main(int argc, char* argv[])
{
    auto bus = sdbusplus::bus::new_default();

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager::manager objManager(bus,
            SOFTWARE_OBJPATH);

    phosphor::software::manager::Delete manager(bus,
            SOFTWARE_OBJPATH);

    bus.request_name(DELETE_BUSNAME);

    while (true)
    {
        bus.process_discard();
        bus.wait();
    }
    return 0;
}
