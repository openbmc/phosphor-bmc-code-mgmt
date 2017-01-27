#include <iostream>
#include <cstdlib>
#include <exception>
#include <sdbusplus/bus.hpp>
#include "config.h"
#include "version_software_manager.hpp"
#include "activation_software_manager.hpp"

int main(int argc, char* argv[])
{
    auto bus = sdbusplus::bus::new_default();

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager::manager objManager(bus,
            SOFTWARE_OBJPATH);

    phosphor::software::manager::Version versionManager(bus,
            SOFTWARE_OBJPATH);

    auto objPathInst = std::string{SOFTWARE_OBJPATH} + '/' +
                       versionManager.id;

    phosphor::software::manager::Activation activationManager(bus,
            objPathInst.c_str());

    bus.request_name(VERSION_BUSNAME);
    bus.request_name(ACTIVATION_BUSNAME);

    while (true)
    {
        bus.process_discard();
        bus.wait();
    }
    return 0;
}
