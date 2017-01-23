#include <iostream>
#include <string>
#include <fstream>
#include <log.hpp>
#include "level_software_manager.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using namespace phosphor::logging;

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Software::server;

void Level::setInitialProperties(const std::string& version,
    const LevelPurpose& purpose)
{
    server::Level::purpose(purpose);

    server::Level::version(version);

    return;
}

std::string Level::version(const std::string value)
{
    log<level::INFO>("Change to Software Version",
                     entry("VERSION=%s",
                           value));

    return server::Level::version(value);
}


Level::LevelPurpose Level::purpose(const LevelPurpose value)
{
    log<level::INFO>("Change to Software Purpose",
                     entry("PURPOSE=%s",
                           convertForMessage(value).c_str()));

    return server::Level::purpose(value);
}

const std::string Level::getVersion()
{
    // Get version from /etc/os-release.
    std::string version{};
    std::ifstream efile;
    std::string line;
    efile.open("/etc/os-release");

    while(getline(efile, line)) {
        if (line.find("VERSION_ID=") != std::string::npos) {
            // This line looks like VERSION_ID="v1.99.0-353-ga3b8a0a-dirty".
            // So grab everything in quotes. 
            version = line.substr(12,26);
            break;
        }
    }
    efile.close();
    return version;
}

} // namespace manager
} // namespace software
} // namepsace phosphor
