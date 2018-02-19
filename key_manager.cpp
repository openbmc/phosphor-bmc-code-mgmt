#include <fstream>
#include <set>
#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include "key_manager.hpp"
#include "version.hpp"

namespace phosphor
{
namespace software
{
namespace image
{
using namespace phosphor::logging;
using namespace phosphor::software::manager;
using InternalFailure =
        sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

constexpr auto keyTypeTag = "KeyType";
constexpr auto hashFunctionTag = "HashType";

HashType KeyManager::getHashTypeFromManifest()
{
    fs::path mainfestFile(_extractPath);
    mainfestFile /= MANIFEST_FILE_NAME;

    return Version::getValue(mainfestFile.string(), hashFunctionTag);
}

HashType KeyManager::getHashTypeFromFile(const HashFilePath& file)
{
    return Version::getValue(file, hashFunctionTag);
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
    if (keyType.empty())
    {
        log<level::ERR>("KeyType tag value not found in manifest file ");
        elog<InternalFailure>();
    }

    fs::path filePath(_signedConfPath);
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
    srcFile /= PUBLICKEY_FILE_NAME;

    if (!fs::exists(srcFile))
    {
        log<level::ERR>("Public key file not found in the image",
                        entry("FILE=%s", srcFile.string().c_str()));
        elog<InternalFailure>();
    }

    // compute destination file based on key type
    // Example: /etc/activationdata/key/OpenBmc/PublicKey.pem
    KeyType keyTypeStr = std::move(getKeyTypeFromManifest());
    fs::path destFile(_signedConfPath);
    destFile /= keyTypeStr;
    fs::create_directories(destFile);

    // add file name
    destFile /= PUBLICKEY_FILE_NAME;

    // copy file to destination
    fs::copy_file(srcFile, destFile);
}


AvailableKeyTypes KeyManager::getAvailableKeyTypesFromSystem()
{
    AvailableKeyTypes keyTypes{};

    //Find the path of all the files
    std::set<fs::path> pathList{};
    if (!fs::is_directory(_signedConfPath))
    {
        log<level::ERR>("Hash file path not found in the system");
        return keyTypes;
    }

    //look for all the hash and public key files
    for(const auto& p: fs::recursive_directory_iterator(_signedConfPath))
    {
        if(p.path().filename() == HASH_FILE_NAME)
        {
            pathList.emplace(p.path().parent_path());
        }
        else if(p.path().filename() == PUBLICKEY_FILE_NAME)
        {
            pathList.emplace(p.path().parent_path());
        }
    }

    //extract the key types
    //  /etc/activationdata/OpenBMC/public  -> get OpenBMC from the path
    for(const auto& key: pathList)
    {
        std::string value{key.parent_path().string()};
        std::size_t found = value.find_last_of("/\\");
        keyTypes.emplace_back(value.substr(found+1));
    }
    return keyTypes;
}

KeyHashPathPair KeyManager::getKeyHashFileNames(const KeyType& key)
{
    fs::path hashpath(_signedConfPath);
    hashpath /= key;
    hashpath /= HASH_FILE_NAME;

    fs::path keyPath(_signedConfPath);
    keyPath /= key;
    keyPath /= PUBLICKEY_FILE_NAME;

    return std::make_pair(hashpath, keyPath);
}

} // namespace image
} // namespace software
} // namespace phosphor
