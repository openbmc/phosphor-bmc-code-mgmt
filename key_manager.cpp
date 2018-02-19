#include <fstream>
#include "key_manager.hpp"
#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <experimental/filesystem>
#include <xyz/openbmc_project/Common/error.hpp>
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

constexpr auto keyTypeTag = "KeyType";
constexpr auto hashFunctionTag = "HashType";

HashType KeyManager::getHashTypeFromManifest()
{
    return getValue(_manifestFile, hashFunctionTag);
}

KeyType KeyManager::getKeyTypeFromManifest()
{
    return getValue(_manifestFile, keyTypeTag);
}

void KeyManager::saveHashTypeToSystem()
{
    // compute destination file based on key type
    // Example: /etc/activationdata/key/OpenBmc/HashType.hash
    KeyType keyType = std::move(getKeyTypeFromManifest());
    fs::path filePath(_hashFilesPath);
    filePath /= keyType;
    fs::create_directories(filePath);

    // add file name
    filePath /= hashFileName;

    //
    // store the hash value
    std::string hashStr = std::move(getHashTypeFromManifest());
    std::fstream outStream(filePath.string(), std::fstream::out);
    outStream << hashFunctionTag << "=" << hashStr << "\n" ;
    outStream.close();
}

void KeyManager::savePublicKeyToSystem(const std::string& srcFile)
{
    //check if source file exists
    fs::path source(srcFile);
    if(!fs::exists(source))
    {
        log<level::ERR>("Error failed to find the source file",
                        entry("SRC_KEY_FILE=%s", source.c_str()));
        elog<InternalFailure>();
    }

    // compute destination file based on key type
    // Example: /etc/activationdata/key/OpenBmc/PublicKey.pem
    KeyType keyTypeStr = std::move(getKeyTypeFromManifest());
    fs::path destFile(_keyFilesPath);
    destFile /= keyTypeStr;
    fs::create_directories(destFile);

    // add file name
    destFile /= publicKeyFileName;

    // copy file to destination
    fs::copy_file(source, destFile);
}

KeyFileList KeyManager::getAllKeyFilePath()
{
    KeyFileList keysPath;
    fs::path pubKeyPath(_keyFilesPath);
    if (!fs::is_directory(pubKeyPath))
    {
        log<level::ERR>("Public key path not found in the system");
        return keysPath;
    }

    for(const auto& p: fs::recursive_directory_iterator(pubKeyPath))
    {
        if(endsWith(p.path().string(), publicKeyFileName))
        {
            keysPath.emplace_back(p.path().string());
        }
    }
    return keysPath;
}

HashFileList KeyManager::getAllHashFilePath()
{
    HashFileList hashPath;
    fs::path hasFilePath(_hashFilesPath);
    if (!fs::is_directory(hasFilePath))
    {
        log<level::ERR>("Hash file path not found in the system");
        return hashPath;
    }

    for(const auto& p: fs::recursive_directory_iterator(hasFilePath))
    {
        if(endsWith(p.path().string(), hashFileName))
        {
            hashPath.emplace_back(p.path().string());
        }
    }
    return hashPath;
}

Value KeyManager::getValue(const std::string& file, const std::string& keyTag)
{
    Value keyValue{};

    // open file
    std::fstream stream(file, std::fstream::in);
    if (!stream.is_open())
    {
        log<level::ERR>("Error failed to open configuration file",
                        entry("CONFIG_FILE=%s", file.c_str()));
        elog<InternalFailure>();
    }

    std::string searchTag = keyTag + "=";
    auto length = searchTag.length();
    for (std::string line; std::getline(stream, line);)
    {
        if (line.compare(0, length, searchTag) == 0)
        {
            keyValue = line.substr(length);
            break;
        }
    }
    return keyValue;
}

bool KeyManager::endsWith(
        const std::string &mainStr, const std::string &toMatch)
{
    if(mainStr.size() >= toMatch.size() &&
        mainStr.compare(mainStr.size() - toMatch.size(),
                        toMatch.size(), toMatch) == 0)
    {
        return true;
    }
    return false;
}

} // namespace manager
} // namespace software
} // namespace phosphor
