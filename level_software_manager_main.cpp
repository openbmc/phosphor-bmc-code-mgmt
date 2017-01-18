#include <iostream>
#include <cstdlib>
#include <exception>
#include <sdbusplus/bus.hpp>
#include "config.h"
#include "level_software_manager.hpp"

int main(int argc, char *argv[])
{
    auto bus = sdbusplus::bus::new_default();

    auto version = phosphor::software::manager::Level::getVersion();

    if(version.empty())
    {
        return -1;
    }

    // Only want 8 digits.
    auto id = std::hash<std::string>{}(version) % 100000000;

    auto objPathInst = std::string{LEVEL_OBJPATH} + '/' +
        std::to_string(id);

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager::manager objManager(bus,
                                                   objPathInst.c_str());

    phosphor::software::manager::Level manager(bus,
                                               objPathInst.c_str());

    bus.request_name(LEVEL_BUSNAME);

    while(true)
    {
        bus.process_discard();
        bus.wait();
    }
    return 0;
}
