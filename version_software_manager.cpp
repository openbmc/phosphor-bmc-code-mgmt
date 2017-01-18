#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include "version_software_manager.hpp"
// 16^8
#define ID_DIVISOR 4294967296

namespace phosphor
{
namespace software
{
namespace manager
{

const std::string Version::getVersion()
{
    // Get version from /etc/os-release.
    std::string versionKey = "VERSION_ID=";
    std::string version{};
    std::ifstream efile;
    std::string line;
    efile.open("/etc/os-release");

    while (getline(efile, line))
    {
        if (line.substr(0, versionKey.size()).find(versionKey)
            != std::string::npos)
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

const std::string Version::getId()
{
    auto version = getVersion();
    std::stringstream hexId;

    if (version.empty())
    {
        return "";
    }

    // Only want 8 hex digits.
    hexId << std::hex << (std::hash<std::string> {}(version)) % ID_DIVISOR;
    return hexId.str();
}

} // namespace manager
} // namespace software
} // namepsace phosphor
