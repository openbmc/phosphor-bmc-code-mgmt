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

Hash_t KeyManager::getHashTypeFromManifest()
{
    fs::path mainfestFile(extractPath);
    mainfestFile /= MANIFEST_FILE_NAME;

    return Version::getValue(mainfestFile.string(), hashFunctionTag);
}

Hash_t KeyManager::getHashTypeFromFile(const HashFilePath& file)
{
    return Version::getValue(file, hashFunctionTag);
}

Key_t KeyManager::getKeyTypeFromManifest()
{
    fs::path mainfestFile(extractPath);
    mainfestFile /= MANIFEST_FILE_NAME;

    return Version::getValue(mainfestFile.string(), keyTypeTag);
}

bool KeyManager::saveHashTypeToSystem()
{
    try
    {
        // compute destination file based on key type
        // Example: /etc/activationdata/key/OpenBmc/HashType.hash
        const Key_t& keyType = getKeyTypeFromManifest();
        if (keyType.empty())
        {
            log<level::ERR>("KeyType tag value not found in manifest file ");
            return false;
        }

        fs::path filePath(signedConfPath);
        filePath /= keyType;
        fs::create_directories(filePath);

        // add file name
        filePath /= HASH_FILE_NAME;

        // store the hash value
        std::string hashStr = getHashTypeFromManifest();
        std::fstream outStream(filePath.string(), std::fstream::out);
        outStream << hashFunctionTag << "=" << hashStr << "\n" ;
        outStream.close();

        //set file permission to 600
        fs::permissions(
            filePath, fs::perms::remove_perms | fs::perms::all);
        fs::permissions(
            filePath, fs::perms::add_perms | fs::perms::owner_read |
            fs::perms::owner_write);
    }
    catch(const std::exception& ex)
    {
        log<level::ERR>("Failure in saving key to system");
        return false;
    }
    return true;
}

bool KeyManager::savePublicKeyToSystem()
{
    try
    {
        //check if source file exists
        fs::path srcFile(extractPath);
        srcFile /= PUBLICKEY_FILE_NAME;

        if (!fs::exists(srcFile))
        {
            log<level::ERR>("Public key file not found in the image",
                            entry("FILE=%s", srcFile.string().c_str()));
            return false;
        }

        // compute destination file based on key type
        // Example: /etc/activationdata/key/OpenBmc/publickey
        Key_t keyType = getKeyTypeFromManifest();
        fs::path destFile(signedConfPath);
        destFile /= keyType;
        fs::create_directories(destFile);

        // add file name
        destFile /= PUBLICKEY_FILE_NAME;

        // copy file to destination
        fs::copy_file(srcFile, destFile);

        //set file permission to 600
        fs::permissions(
            destFile, fs::perms::remove_perms | fs::perms::all);
        fs::permissions(
            destFile, fs::perms::add_perms | fs::perms::owner_read |
            fs::perms::owner_write);
    }
    catch(const std::exception& ex)
    {
        log<level::ERR>("Failure in saving public key to system");
        return false;
    }
    return true;
}


AvailableKeyTypes KeyManager::getAvailableKeyTypesFromSystem()
{
    AvailableKeyTypes keyTypes{};

    //Find the path of all the files
    std::set<fs::path> pathList{};
    if (!fs::is_directory(signedConfPath))
    {
        log<level::ERR>("Hash file path not found in the system");
        return keyTypes;
    }

    // Look for all the hash and public key files and then get the parent paths
    // and store it in set. Using set to remove duplicates.
    // For example:
    // /etc/activationdata/OpenBMC/publickey
    // /etc/activationdata/OpenBMC/hashfunc
    // Set will have /etc/activationdata/OpenBMC/
    for(const auto& p: fs::recursive_directory_iterator(signedConfPath))
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

    // extract the key types
    // /etc/activationdata/OpenBMC/  -> get OpenBMC from the path
    for(const auto& key: pathList)
    {
        std::string value{key.parent_path().string()};
        std::size_t found = value.find_last_of("/\\");
        keyTypes.emplace_back(value.substr(found+1));
    }
    return keyTypes;
}

KeyHashPathPair KeyManager::getKeyHashFileNames(const Key_t& key)
{
    fs::path hashpath(signedConfPath);
    hashpath /= key;
    hashpath /= HASH_FILE_NAME;

    fs::path keyPath(signedConfPath);
    keyPath /= key;
    keyPath /= PUBLICKEY_FILE_NAME;

    return std::make_pair(hashpath, keyPath);
}

} // namespace image
} // namespace software
} // namespace phosphor
