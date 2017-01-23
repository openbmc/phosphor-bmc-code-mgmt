#include <iostream>
#include <string>
#include <fstream>
#include "version_software_manager.hpp"
#define ID_DIVISOR 100000000

namespace phosphor
{
namespace software
{
namespace manager
{

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Software::server;

const std::string Version::getVersion()
{
    // Get version from /etc/os-release.
    std::string version{};
    std::ifstream efile;
    std::string line;
    efile.open("/etc/os-release");

    while (getline(efile, line))
    {
        if (line.substr(0, 11).find("VERSION_ID=") != std::string::npos)
        {
            // This line looks like VERSION_ID="v1.99.0-353-ga3b8a0a-dirty".
            // So grab everything in quotes.
            std::size_t pos = line.find_first_of('"', 11) + 1;
            version = line.substr(pos, line.find_last_of('"') - pos);
            break;
        }
    }
    efile.close();

    return version;
}

const size_t Version::getId()
{
    auto version = getVersion();

    if (version.empty())
    {
        return -1;
    }

    // Only want 8 digits.
    return std::hash<std::string> {}(version) % ID_DIVISOR;
}

} // namespace manager
} // namespace software
} // namepsace phosphor
