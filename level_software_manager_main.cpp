#include <fstream>
#include <cstdlib>
#include <iostream>
#include <exception>
#include <sdbusplus/bus.hpp>
#include "config.h"
#include "level_software_manager.hpp"

int main(int argc, char *argv[])
{
    auto bus = sdbusplus::bus::new_default();

    // Use hash of /etc/os-release to determine id.
    std::ifstream efile;
    std::string line;
    std::string version;
    efile.open("/etc/os-release");

    while(!efile.eof()) {
        getline(efile, line);
        if (line.find("VERSION=") != std::string::npos) {
            version = line.substr(9,11);
            break;
        }
    }
    efile.close();

    // Only want 8 digits.
    auto id = std::hash<std::string>{}(version) % 100000000;

    // For now, we only have one instance of the level
    auto objPathInst = std::string(LEVEL_OBJPATH) + '/' + std::to_string(id);

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager::manager objManager(bus,
                                                   objPathInst.c_str());

    phosphor::software::manager::Level::Properties properties;
    properties.version = version;

    // For now, we only support the BMC code level
    properties.purpose = phosphor::software::manager::Level::LevelPurpose::BMC;

    phosphor::software::manager::Level manager(bus,
                                               objPathInst.c_str(),
                                               properties);

    bus.request_name(LEVEL_BUSNAME);

    while(true)
    {
        bus.process_discard();
        bus.wait();
    }
    return 0;
}
