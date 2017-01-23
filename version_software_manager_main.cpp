#include <iostream>
#include <cstdlib>
#include <exception>
#include <sdbusplus/bus.hpp>
#include "config.h"
#include "version_software_manager.hpp"
#define ID_DIVISOR 100000000

int main(int argc, char* argv[])
{
    auto bus = sdbusplus::bus::new_default();

    auto version = phosphor::software::manager::Version::getVersion();

    if (version.empty())
    {
        return -1;
    }

    // Only want 8 digits.
    auto id = std::hash<std::string> {}(version) % ID_DIVISOR;

    auto objPathInst = std::string{VERSION_OBJPATH} + '/' + std::to_string(id);

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager::manager objManager(bus,
            objPathInst.c_str());

    phosphor::software::manager::Version::Properties properties;
    properties.version = version;

    // For now, we only support the BMC code version
    properties.purpose = phosphor::software::manager::
                         Version::VersionPurpose::BMC;

    phosphor::software::manager::Version manager(bus,
            objPathInst.c_str(),
            properties);

    bus.request_name(VERSION_BUSNAME);

    while (true)
    {
        bus.process_discard();
        bus.wait();
    }
    return 0;
}
