#include <iostream>
#include <string>
#include <fstream>
#include "version_software_manager.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

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
