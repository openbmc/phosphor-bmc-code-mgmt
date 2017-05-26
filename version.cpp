#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <phosphor-logging/log.hpp>
#include "version.hpp"
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include "xyz/openbmc_project/Common/error.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;

std::string Version::getValue(const std::string& manifestFilePath,
                              std::string key)
{
    key = key + "=";
    auto keySize = key.length();

    if (manifestFilePath.empty())
    {
        log<level::ERR>("Error MANIFESTFilePath is empty");
        throw std::runtime_error("MANIFESTFilePath is empty");
    }

    std::string value{};
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
            if (line.compare(0, keySize, key) == 0)
            {
                value = line.substr(keySize);
                break;
            }
        }
        efile.close();
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Error in reading MANIFEST file");
    }

    return value;
}

std::string Version::getId(const std::string& version)
{
    std::stringstream hexId;

    if (version.empty())
    {
        log<level::ERR>("Error version is empty");
        throw std::runtime_error("Version is empty");
    }

    // Only want 8 hex digits.
    hexId << std::hex << ((std::hash<std::string> {}(
                               version)) & 0xFFFFFFFF);
    return hexId.str();
}

std::string Version::getBMCVersion()
{
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
            std::size_t pos = line.find_first_of('"') + 1;
            version = line.substr(pos, line.find_last_of('"') - pos);
            break;
        }
    }
    efile.close();
    return version;
}

std::string Version::getBMCId()
{
    auto bmcVersion = getBMCVersion();
    std::stringstream hexId;

    if (bmcVersion.empty())
    {
        log<level::ERR>("Error bmcversion is empty");
        elog<InvalidArgument>(xyz::openbmc_project::Common::InvalidArgument::
                              ARGUMENT_NAME("BMCVersion"),
                              xyz::openbmc_project::Common::InvalidArgument::
                              ARGUMENT_VALUE(bmcVersion.c_str()));
        return NULL;
        
    }
    hexId << std::hex << ((std::hash<std::string> {}(bmcVersion)) & 0xFFFFFFFF);
    return hexId.str();
}

} // namespace manager
} // namespace software
} // namepsace phosphor

