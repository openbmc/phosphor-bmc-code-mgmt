#include <fstream>
#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <experimental/filesystem>
#include <xyz/openbmc_project/Common/error.hpp>
#include "key_manager.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{
using namespace phosphor::logging;
namespace fs = std::experimental::filesystem;
using InternalFailure =
        sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

constexpr auto publicKeyPathTag = "publickeypath=";
constexpr auto publicKeyBeginTag = "-----BEGIN PUBLIC KEY-----";
constexpr auto publicKeyEndTag = "-----END PUBLIC KEY-----";

std::string KeyManager::getPublicKeyPath(const std::string& manifestFile)
{
    std::string path{};

    // open manifest file
    std::fstream manifestStream(manifestFile, std::fstream::in);
    if (!manifestStream.is_open())
    {
        log<level::ERR>("Error failed to open MANIFEST file",
                        entry("MANIFEST_FILE=%s", manifestFile.c_str()));
        elog<InternalFailure>();
    }

    auto length = strlen(publicKeyPathTag);
    for (std::string line; std::getline(manifestStream, line);)
    {
        if (line.compare(0, length, publicKeyPathTag) == 0)
        {
            path = line.substr(length);
        }
    }
    return path;
}

void KeyManager::writePublicKey(const std::string& manifestFile,
                           const std::string& pubKeyFile)
{
    // open manifest file
    std::fstream manifestStream(manifestFile, std::fstream::in);
    if (!manifestStream.is_open())
    {
        log<level::ERR>("Error failed to open MANIFEST file",
                        entry("MANIFEST_FILE=%s", manifestFile.c_str()));
        elog<InternalFailure>();
    }

    // create a public key file in the specified path
    std::fstream outStream(pubKeyFile, std::fstream::out);
    if (!outStream.is_open())
    {
        log<level::ERR>("Error failed to open public key file",
                        entry("PUBLICKEY_FILE=%s", pubKeyFile.c_str()));
        elog<InternalFailure>();
    }

    // check for the public key tags and write to public key file
    bool beginKey = false;
    for (std::string line; std::getline(manifestStream, line);)
    {
        if (!beginKey)
        {
            size_t index = line.find(publicKeyBeginTag);
            if (index != std::string::npos)
            {
                outStream << line << "\n" ;
                beginKey = true;
            }
        }
        else
        {
            size_t index = line.find(publicKeyEndTag);
            if (index != std::string::npos)
            {
                outStream << line << "\n" ;
                break;
            }
            outStream << line << "\n" ;
        }
    }
}

bool KeyManager::isPublicKeyFound(const std::string& manifestFile)
{
    // open manifest file
    std::fstream manifestStream(manifestFile, std::fstream::in);
    if (!manifestStream.is_open())
    {
        return false;
    }

    // check if public key tags are found in the file
    bool beginKey = false;
    bool endKey = false;
    for (std::string line; std::getline(manifestStream, line);)
    {
        if (!beginKey)
        {
            size_t index = line.find(publicKeyBeginTag);
            if (index != std::string::npos)
            {
                beginKey = true;
            }
        }
        else
        {
            size_t index = line.find(publicKeyEndTag);
            if (index != std::string::npos)
            {
                endKey = true;
                break;
            }
        }
    }

    if (beginKey && endKey)
    {
        return true;
    }
    return false;
}
} // namespace manager
} // namespace software
} // namespace phosphor
