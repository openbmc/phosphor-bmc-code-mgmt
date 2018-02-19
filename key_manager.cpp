#include <fstream>
#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog.hpp>
#include "key_manager.hpp"
#include "version.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{
using namespace phosphor::logging;

constexpr auto keyTypeTag = "KeyType";
constexpr auto hashFunctionTag = "HashType";

HashType KeyManager::getHashTypeFromManifest()
{
    fs::path mainfestFile(_extractPath);
    mainfestFile /= MANIFEST_FILE_NAME;

    return Version::getValue(mainfestFile.string(), hashFunctionTag);
}

KeyType KeyManager::getKeyTypeFromManifest()
{
    fs::path mainfestFile(_extractPath);
    mainfestFile /= MANIFEST_FILE_NAME;

    return Version::getValue(mainfestFile.string(), keyTypeTag);
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
    filePath /= HASH_FILE_NAME;

    // store the hash value
    std::string hashStr = std::move(getHashTypeFromManifest());
    std::fstream outStream(filePath.string(), std::fstream::out);
    outStream << hashFunctionTag << "=" << hashStr << "\n" ;
    outStream.close();
}

void KeyManager::savePublicKeyToSystem()
{
    //check if source file exists
    fs::path srcFile(_extractPath);
    srcFile /= PUBLIC_KEY_FILE_NAME;

    // compute destination file based on key type
    // Example: /etc/activationdata/key/OpenBmc/PublicKey.pem
    KeyType keyTypeStr = std::move(getKeyTypeFromManifest());
    fs::path destFile(_pubKeyFilesPath);
    destFile /= keyTypeStr;
    fs::create_directories(destFile);

    // add file name
    destFile /= PUBLIC_KEY_FILE_NAME;

    // copy file to destination
    fs::copy_file(srcFile, destFile);
}

PubKeyPathList KeyManager::getAllPubKeyFilePath()
{
    PubKeyPathList keysPath;
    fs::path pubKeyPath(_pubKeyFilesPath);
    if (!fs::is_directory(pubKeyPath))
    {
        log<level::ERR>("Public key path not found in the system");
        return keysPath;
    }

    for(const auto& p: fs::recursive_directory_iterator(pubKeyPath))
    {
        if(p.path().filename() == PUBLIC_KEY_FILE_NAME)
        {
            keysPath.emplace_back(p.path().string());
        }
    }
    return keysPath;
}

HashFilePathList KeyManager::getAllHashFilePath()
{
    HashFilePathList hashPath;
    if (!fs::is_directory(_hashFilesPath))
    {
        log<level::ERR>("Hash file path not found in the system");
        return hashPath;
    }

    for(const auto& p: fs::recursive_directory_iterator(_hashFilesPath))
    {
        if(p.path().filename() == HASH_FILE_NAME)
        {
            hashPath.emplace_back(p.path().string());
        }
    }
    return hashPath;
}
} // namespace manager
} // namespace software
} // namespace phosphor
