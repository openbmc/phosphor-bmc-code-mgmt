#include <set>

#include "image_verify.hpp"
#include "config.h"
#include "version.hpp"

#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

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

Signature::Signature(const fs::path& imageDirPath,
                     const fs::path& signedConfPath) :
    imageDirPath(imageDirPath),
    signedConfPath(signedConfPath),
    publicKeyFile(imageDirPath / PUBLICKEY_FILE_NAME)
{
    fs::path file(imageDirPath / MANIFEST_FILE_NAME);

    keyType = Version::getValue(file, keyTypeTag);
    hashType = Version::getValue(file, hashFunctionTag);
}

inline const AvailableKeyTypes Signature::getAvailableKeyTypesFromSystem()
{
    AvailableKeyTypes keyTypes{};

    // Find the path of all the files
    std::set<fs::path> pathList{};
    if (!fs::is_directory(signedConfPath))
    {
        log<level::ERR>("Hash file path not found in the system");
        elog<InternalFailure>();
    }

    // Look for all the hash and public key files and then get the parent paths
    // and store it in set. Using set to remove duplicates.
    // For example:
    // /etc/activationdata/OpenBMC/publickey
    // /etc/activationdata/OpenBMC/hashfunc
    // Set will have /etc/activationdata/OpenBMC/
    for (const auto& p : fs::recursive_directory_iterator(signedConfPath))
    {
        if ((p.path().filename() == HASH_FILE_NAME) ||
            (p.path().filename() == PUBLICKEY_FILE_NAME))
        {
            pathList.emplace(p.path().parent_path());
        }
    }

    // extract the key types
    // /etc/activationdata/OpenBMC/  -> get OpenBMC from the path
    for (const auto& key : pathList)
    {
        std::string value{key.string()};
        std::size_t found = value.find_last_of("/\\");
        keyTypes.emplace_back(value.substr(found + 1));
    }
    return keyTypes;
}

KeyHashPathPair Signature::getKeyHashFileNames(const Key_t& key)
{
    fs::path hashpath(signedConfPath / key / HASH_FILE_NAME);
    fs::path keyPath(signedConfPath / key / PUBLICKEY_FILE_NAME);

    return std::make_pair(hashpath, keyPath);
}

inline const Hash_t Signature::getHashTypeFromFile(const HashFilePath& file)
{
    return Version::getValue(file, hashFunctionTag);
}

bool Signature::verify()
{
    try
    {
        bool valid = false;

        // Verify the MANIFEST file using available public keys and hash
        // on the system.
        if (false == systemLevelVerify(MANIFEST_FILE_NAME))
        {
            log<level::ERR>("Manifest file Signature Validation failed");
            return false;
        }

        //  Verify the publickey file using available public keys and hash
        //  on the system
        if (false == systemLevelVerify(PUBLICKEY_FILE_NAME))
        {
            log<level::ERR>("Public Key file Signature Validation failed");
            return false;
        }

        //Validate the BMC image files.
        for (const auto& bmcImage : bmcImages)
        {
            //Build Image File name
            fs::path file(imageDirPath);
            file /= bmcImage;

            //Build Signature File name
            fs::path sigFile(file);
            sigFile.replace_extension(SIGNATURE_FILE_EXT);

            //Verify the signature.
            valid = verifyFile(file,
                               sigFile,
                               publicKeyFile,
                               hashType);
            if (valid == false)
            {
                log<level::ERR>("Image file Signature Validation failed",
                                entry("IMAGE=%s", bmcImage.c_str()));
                return false;
            }
        }

        log<level::INFO>("Sucessfully completed Signature vaildation.");

        return true;
    }
    catch (InternalFailure& e)
    {
        return false;
    }
}


bool Signature::systemLevelVerify(const std::string& name)
{
    //Build File complete path
    fs::path file(imageDirPath);
    file /= name;

    //Build Signature File name
    fs::path sigFile(file);
    sigFile.replace_extension(SIGNATURE_FILE_EXT);

    //Get available key types from the system.
    auto keyTypes = getAvailableKeyTypesFromSystem();
    if (keyTypes.empty())
    {
        log<level::ERR>("Missing Signature configuration data in system");
        elog<InternalFailure>();
    }

    bool valid = false;

    //Verify the file signature with available key types
    //public keys and hash function.
    for (const auto& keyType : keyTypes)
    {
        auto keyHashPair = getKeyHashFileNames(keyType);

        auto hashFunc = getHashTypeFromFile(keyHashPair.first);

        valid = verifyFile(file,
                           sigFile,
                           keyHashPair.second,
                           hashFunc);
        if (valid)
        {
            break;
        }
    }
    return valid;
}

bool Signature::verifyFile(const fs::path& file,
                           const fs::path& sigFile,
                           const fs::path& publicKey,
                           const std::string& hashFunc)
{
    return true;
}

} //namespace image
} //namespace software
} //namespace phosphor
