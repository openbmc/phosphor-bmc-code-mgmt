#include "image_verify.hpp"
#include "config.h"

#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog.hpp>

#include <fstream>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace phosphor
{
namespace software
{
namespace image
{

using namespace phosphor::logging;

using RSA_Ptr = std::unique_ptr<RSA, decltype(&::RSA_free)>;
using BIO_MEM_Ptr = std::unique_ptr<BIO, decltype(&::BIO_free)>;
using EVP_PKEY_Ptr = std::unique_ptr<EVP_PKEY,
      decltype(&::EVP_PKEY_free)>;
using EVP_MD_CTX_Ptr = std::unique_ptr<EVP_MD_CTX,
      decltype(&::EVP_MD_CTX_destroy)>;

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

    //Check existence of the files in the system.
    if (!(fs::exists(file) && fs::exists(sigFile)))
    {
        log<level::ERR>("Failed to find the Data or signature file.",
                        entry("File=%s", file.c_str()));
        return false;
    }

    //Create RSA.
    RSA_Ptr publicRSA(createPublicRSA(publicKey), ::RSA_free);
    if (publicRSA.get() == NULL)
    {
        log<level::ERR>("Failed to create RSA",
                        entry("File=%s", publicKey.c_str()));
        return false;
    }

    //Assign public key to RSA.
    EVP_PKEY* pubKey  = EVP_PKEY_new();
    EVP_PKEY_assign_RSA(pubKey, publicRSA.get());

    //Initializes a digest context.
    EVP_MD_CTX_Ptr rsaVerifyCtx(EVP_MD_CTX_create(), ::EVP_MD_CTX_destroy);

    //Adds all digest algorithms to the internal table
    OpenSSL_add_all_digests();

    //Create  Hash structre.
    const EVP_MD* hashPtr = EVP_get_digestbyname(hashFunc.c_str());
    if (!hashPtr)
    {
        log<level::ERR>("EVP_get_digestbynam: Unknown message digest",
                        entry("HASH=%s", hashFunc.c_str()));
        return false;
    }

    auto result = EVP_DigestVerifyInit(rsaVerifyCtx.get(), NULL,
                                       hashPtr, NULL, pubKey);

    if (result <= 0)
    {
        log<level::ERR>("Error occured during EVP_DigestVerifyInit",
                        entry("ERRNO=%d", result));
        return false;
    }

    //Hash the data file and update the verification context
    auto size = fs::file_size(file);
    auto dataPtr = mapFile(file, size);

    result = EVP_DigestVerifyUpdate(rsaVerifyCtx.get(), dataPtr(), size);
    if (result <= 0)
    {
        log<level::ERR>("Error occured during EVP_DigestVerifyUpdate",
                        entry("ERRNO=%d", result));
        return false;
    }

    //Verify the data with signature.
    size = fs::file_size(sigFile);
    auto signature = mapFile(sigFile, size);

    result = EVP_DigestVerifyFinal(rsaVerifyCtx.get(),
                                   reinterpret_cast<unsigned char*>(signature()),
                                   size);

    //cleans up digest context.
    EVP_MD_CTX_cleanup(rsaVerifyCtx.get());

    //Check the verification result.
    if (result < 0)
    {
        log<level::ERR>("Error occured during EVP_DigestVerifyFinal",
                        entry("ERRNO=%d", result));
        return false;
    }
    else if (result == 0)
    {
        log<level::INFO>("EVP_DigestVerifyFinal:Signature validation failed",
                         entry("path=%s", sigFile.c_str()));
        return false;
    }
    return true;
}

inline RSA* Signature::createPublicRSA(const fs::path& publicKey)
{
    RSA* rsa = NULL;
    auto size = fs::file_size(publicKey);

    //Read public key file
    auto data = mapFile(publicKey, size);

    BIO_MEM_Ptr keyBio(BIO_new_mem_buf(data(), -1), &::BIO_free);
    if (keyBio.get() != NULL)
    {
        //Create RSA
        rsa = PEM_read_bio_RSA_PUBKEY(keyBio.get(), &rsa, NULL, NULL);
    }

    return rsa;
}

CustomMap Signature::mapFile(const fs::path& path, size_t size)
{

    CustomFd fd(open(path.c_str(), O_RDONLY));

    return CustomMap(mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd(), 0), size);
}

} //namespace image
} //namespace software
} //namespace phosphor
