#include <iostream>
#include <string>
#include <fstream>
#include "version_software_manager.hpp"
#include <log.hpp>

namespace phosphor
{
namespace software
{
namespace manager
{

using namespace phosphor::logging;

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Software::server;

void Version::setInitialProperties(const std::string& version,
                                   VersionPurpose purpose)
{
    server::Version::purpose(purpose);

    server::Version::version(version);

    return;
}

std::string Version::version(const std::string value)
{
    log<level::INFO>("Change to Software Version",
                     entry("VERSION=%s",
                           value));

    return server::Version::version(value);
}


Version::VersionPurpose Version::purpose(VersionPurpose value)
{
    log<level::INFO>("Change to Software Purpose",
                     entry("PURPOSE=%s",
                           convertForMessage(value).c_str()));

    return server::Version::purpose(value);
}

const std::string Version::getVersion()
{
    // Get version from /etc/os-release.
    std::string version{};
    std::ifstream efile;
    std::string line;
    efile.open("/etc/os-release");

    while (getline(efile, line))
    {
        if (line.find("VERSION_ID=") != std::string::npos)
        {
            // This line looks like VERSION_ID="v1.99.0-353-ga3b8a0a-dirty".
            // So grab everything in quotes.
            std::size_t pos = line.find_first_of('"') + 1;
            version = line.substr(pos, line.find_last_of('"') - pos);
            break;
        }
    }
    efile.close();
    return version;
}

} // namespace manager
} // namespace software
} // namepsace phosphor
