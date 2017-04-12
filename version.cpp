#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <phosphor-logging/log.hpp>
#include "version.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using namespace phosphor::logging;

std::string Version::getVersion(const std::string& manifestFilePath)
{
    constexpr auto versionKey = "version=";
    constexpr auto versionKeySize = strlen(versionKey);

    if (manifestFilePath.empty())
    {
        log<level::ERR>("Error MANIFESTFilePath is empty");
        throw std::runtime_error("MANIFESTFilePath is empty");
    }

    std::string version{};
    std::ifstream efile;
    std::string line;
    efile.exceptions(std::ifstream::failbit
                     | std::ifstream::badbit
                     | std::ifstream::eofbit);

    // Too many GCC bugs (53984, 66145) to do this the right way...
    try
    {
        efile.open(manifestFilePath);
        while (getline(efile, line))
        {
            if (line.compare(0, versionKeySize, versionKey) == 0)
            {
                version = line.substr(versionKeySize);
                break;
            }
        }
        efile.close();
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Error in reading Host MANIFEST file");
    }

    return version;
}

std::string Version::getId(const std::string& version)
{
    std::stringstream hexId;

    if (version.empty())
    {
        log<level::ERR>("Error Host version is empty");
        throw std::runtime_error("Host version is empty");
    }

    // Only want 8 hex digits.
    hexId << std::hex << ((std::hash<std::string> {}(
                               version)) & 0xFFFFFFFF);
    return hexId.str();
}

} // namespace manager
} // namespace software
} // namepsace phosphor

