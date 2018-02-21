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
    signedConfPath(signedConfPath)
{
    fs::path file(imageDirPath / MANIFEST_FILE_NAME);

    keyType = Version::getValue(file, keyTypeTag);
    hashType = Version::getValue(file, hashFunctionTag);
}

AvailableKeyTypes Signature::getAvailableKeyTypesFromSystem() const
{
    AvailableKeyTypes keyTypes{};

    // Find the path of all the files
    if (!fs::is_directory(signedConfPath))
    {
        log<level::ERR>("Signed configuration path not found in the system");
        elog<InternalFailure>();
    }

    // Look for all the hash and public key file names get the key value
    // For example:
    // /etc/activationdata/OpenBMC/publickey
    // /etc/activationdata/OpenBMC/hashfunc
    // /etc/activationdata/GA/publickey
    // /etc/activationdata/GA/hashfunc
    // Set will have OpenBMC, GA

    for (const auto& p : fs::recursive_directory_iterator(signedConfPath))
    {
        if ((p.path().filename() == HASH_FILE_NAME) ||
            (p.path().filename() == PUBLICKEY_FILE_NAME))
        {
            // extract the key types
            // /etc/activationdata/OpenBMC/  -> get OpenBMC from the path
            auto key = p.path().parent_path();
            keyTypes.insert(key.filename());
        }
    }

    return keyTypes;
}

inline KeyHashPathPair Signature::getKeyHashFileNames(const Key_t& key) const
{
    fs::path hashpath(signedConfPath / key / HASH_FILE_NAME);
    fs::path keyPath(signedConfPath / key / PUBLICKEY_FILE_NAME);

    return std::make_pair(std::move(hashpath), std::move(keyPath));
}

bool Signature::verify()
{
    try
    {
        // Verify the MANIFEST and publickey file using available
        // public keys and hash on the system.
        if (false == systemLevelVerify())
        {
            log<level::ERR>("System level Signature Validation failed");
            return false;
        }

        // image specfic publickey file name.
        fs::path publicKeyFile(imageDirPath / PUBLICKEY_FILE_NAME);

        // Validate the BMC image files.
        for (const auto& bmcImage : bmcImages)
        {
            // Build Image File name
            fs::path file(imageDirPath);
            file /= bmcImage;

            // Build Signature File name
            fs::path sigFile(file);
            sigFile.replace_extension(SIGNATURE_FILE_EXT);

            // Verify the signature.
            auto valid = verifyFile(file, sigFile, publicKeyFile, hashType);
            if (valid == false)
            {
                log<level::ERR>("Image file Signature Validation failed",
                                entry("IMAGE=%s", bmcImage.c_str()));
                return false;
            }
        }

        log<level::DEBUG>("Sucessfully completed Signature vaildation.");

        return true;
    }
    catch (const InternalFailure& e)
    {
        return false;
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(e.what());
        return false;
    }
}

bool Signature::systemLevelVerify()
{
    // Get available key types from the system.
    auto keyTypes = getAvailableKeyTypesFromSystem();
    if (keyTypes.empty())
    {
        log<level::ERR>("Missing Signature configuration data in system");
        elog<InternalFailure>();
    }

    // Build publickey and its signature file name.
    fs::path pkeyFile(imageDirPath / PUBLICKEY_FILE_NAME);
    fs::path pkeyFileSig(pkeyFile);
    pkeyFileSig.replace_extension(SIGNATURE_FILE_EXT);

    // Build manifest and its signature file name.
    fs::path manifestFile(imageDirPath / MANIFEST_FILE_NAME);
    fs::path manifestFileSig(manifestFile);
    manifestFileSig.replace_extension(SIGNATURE_FILE_EXT);

    auto valid = false;

    // Verify the file signature with available key types
    // public keys and hash function.
    for (const auto& keyType : keyTypes)
    {
        auto keyHashPair = getKeyHashFileNames(keyType);

        auto hashFunc = Version::getValue(keyHashPair.first, hashFunctionTag);

        // Verify manifest file signature
        valid = verifyFile(manifestFile, manifestFileSig, keyHashPair.second,
                           hashFunc);
        if (valid)
        {
            // Verify publickey file signature.
            valid =
                verifyFile(pkeyFile, pkeyFileSig, keyHashPair.second, hashFunc);
            if (valid)
            {
                break;
            }
        }
    }
    return valid;
}

bool Signature::verifyFile(const fs::path& file, const fs::path& sigFile,
                           const fs::path& publicKey,
                           const std::string& hashFunc)
{
    return true;
}

} // namespace image
} // namespace software
} // namespace phosphor
