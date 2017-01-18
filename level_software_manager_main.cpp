#include <cstdlib>
#include <iostream>
#include <exception>
#include <sdbusplus/bus.hpp>
#include "config.h"
#include "level_software_manager.hpp"

int main(int argc, char *argv[])
{
    auto bus = sdbusplus::bus::new_default();

    phosphor::software::manager::Level manager(bus,
                                           BUSNAME,
                                           OBJPATH);
    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager::manager objManager(bus, OBJPATH);

    bus.request_name(BUSNAME);

    while(true)
    {
        bus.process_discard();
        bus.wait();
    }
    return 0;
}
