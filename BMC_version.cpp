#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include "BMC_version.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

const std::string BMCVersion::getVersion() const
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

const std::string BMCVersion::getId() const
{
    auto version = getVersion();
    std::stringstream hexId;

    if (version.empty())
    {
        throw std::runtime_error("Software version is empty");
    }

    // Only want 8 hex digits.
    hexId << std::hex << ((std::hash<std::string> {}(version)) & 0xFFFFFFFF);
    return hexId.str();
}

} // namespace manager
} // namespace software
} // namepsace phosphor
