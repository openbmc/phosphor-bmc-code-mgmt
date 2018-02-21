#include "image_verify.hpp"
#include "config.h"

#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog.hpp>

namespace phosphor
{
namespace software
{
namespace image
{

using namespace phosphor::logging;

bool Signature::verify()
{
    bool valid = false;

    // Verify the MANIFEST file using available public keys and hash
    // on the system.
    if (false == primaryValidation(MANIFEST_FILE_NAME))
    {
        //TODO Add Error handler
        log<level::ERR>("Manifest file Signature Validation failed");
        return false;
    }

    //  Verify the publickey file using available public keys and hash
    //  on the system
    if (false == primaryValidation(PUBLICKEY_FILE_NAME))
    {
        //TODO Add Error handler
        log<level::ERR>("Public Key file Signature Validation failed");
        return false;
    }

    //Get hash function information from the Manifest file
    auto hashFunction = keyManager.getHashTypeFromManifest();

    //Get Image specific public Key file.
    fs::path publicKey(imageDirPath);
    publicKey /= PUBLICKEY_FILE_NAME;

    //Validate the BMC image files.
    for (const auto& bmcImage : bmcImages)
    {
        //Build Image File name
        fs::path file(imageDirPath);
        file /= bmcImage;

        //Build Signature File name
        fs::path sigFile(imageDirPath);
        sigFile /= bmcImage;
        sigFile.replace_extension(SIGNATURE_FILE_EXT);

        //Verify the signature.
        valid = verifyFile(file,
                           sigFile,
                           publicKey,
                           hashFunction);
        if (valid == false)
        {
            //TODO Add Error handler
            log<level::ERR>("Image file Signature Validation failed",
                            entry("IMAGE=%s", bmcImage.c_str()));
            return false;
        }
    }

    log<level::INFO>("Sucessfully completed Signature vaildation.");

    return true;
}


bool Signature::primaryValidation(const std::string& name)
{
    //Build File complete path
    fs::path file(imageDirPath);
    file /= name;

    //Build Signature File name
    fs::path sigFile(imageDirPath);
    sigFile /= name;
    sigFile.replace_extension(SIGNATURE_FILE_EXT);

    //Get available key types from the system.
    auto keyTypes = keyManager.getAvailableKeyTypesFromSystem();
    if (keyTypes.empty())
    {
        log<level::ERR>("Missing Signature configuration data in the system");
        return false;
    }

    bool valid = false;

    //Verify the file signature with available key types
    //public keys and hash function.
    for (const auto& keyType : keyTypes)
    {
        auto keyHashPair = keyManager.getKeyHashFileNames(keyType);

        auto hashFunc = keyManager.getHashTypeFromFile(keyHashPair.first);

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
