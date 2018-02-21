#include "image_verify.hpp"
#include "config.h"

#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>

#include <fstream>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

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

    // Do Available system key based validation of MANIFEST file.
    if (false == primaryValidation(MANIFEST_FILE_NAME))
    {
        //TODO Add Error handler
        log<level::ERR>("Manifest file Signature Validation failed");
        return false;
    }

    // Do Available system key based validation of Public Key file.
    if (false == primaryValidation(PUBLICKEY_FILE_NAME))
    {
        //TODO Add Error handler
        log<level::ERR>("Public Key file Signature Validation failed");
        return false;
    }

    //Get public key and hash function information
    //from the Manifest file and update.

    //Validate the BMC image files.
    for (auto& bmcImage : bmcImages)
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
    bool valid = false;

    //Build File complete path
    fs::path file(imageDirPath);
    file /= name;

    //Build Signature File name
    fs::path sigFile(imageDirPath);
    sigFile /= name;
    sigFile.replace_extension(SIGNATURE_FILE_EXT);

    std::vector<fs::path> publicKeys;
    std::string hashFunc;

    //Get available public Keys ind hash function from the system.
    //TODO Replace with Key manager function.
    publicKeys.push_back(publicKey);
    hashFunc = hashFunction;

    //Verify the manifest file signature with available public keys.
    for (auto& pKey : publicKeys)
    {
        valid = verifyFile(file,
                           sigFile,
                           pKey,
                           hashFunc);
        if (valid)
        {
            break;
        }
    }

    return valid;
}

bool Signature::verifyFile(fs::path file,
                           fs::path sigFile,
                           fs::path publicKey,
                           const std::string& hashFunc)
{

    //Check existence of the files in the system.
    if (!(fs::exists(file) && fs::exists(sigFile)))
    {
        log<level::ERR>("Failed to find the Data or signature file.",
                        entry("File=%s", file.c_str()));
        return false;
    }

    RSA* publicRSA = createPublicRSA(publicKey);
    EVP_PKEY* pubKey  = EVP_PKEY_new();

    EVP_PKEY_assign_RSA(pubKey, publicRSA);

    EVP_MD_CTX* rsaVerifyCtx = EVP_MD_CTX_create();

    OpenSSL_add_all_digests();

    //Create  Hash structre.
    const EVP_MD* hashPtr = EVP_get_digestbyname(hashFunction.c_str());
    if (!hashPtr)
    {
        log<level::ERR>("EVP_get_digestbynam: Unknown message digest",
                        entry("HASH=%s", hashFunction.c_str()));
        return false;
    }

    auto result = EVP_DigestVerifyInit(rsaVerifyCtx, NULL,
                                       hashPtr, NULL, pubKey);

    if (result <= 0)
    {
        log<level::ERR>("Error occured during EVP_DigestVerifyInit",
                        entry("ERRNO=%d", result));
        return false;
    }

    //Data file usually in the range of MB
    //Memory Map is better for EVP data processing.
    size_t size = fs::file_size(file);

    void* data = mapFile(file, size);

    result = EVP_DigestVerifyUpdate(rsaVerifyCtx,
                   reinterpret_cast< void const* >(data), size);
    if (result <= 0)
    {
        log<level::ERR>("Error occured during EVP_DigestVerifyUpdate",
                        entry("ERRNO=%d", result));
        munmap(data, size);
        return false;
    }

    //Get Signature file size and copy into buffer.
    size = fs::file_size(sigFile);

    std::vector<uint8_t> signature ;
    getFileContent(sigFile, signature);

    result = EVP_DigestVerifyFinal(rsaVerifyCtx,
                   reinterpret_cast< uint8_t const* >(signature.data()),
                   size);

    //Remove the mapping.
    munmap(data, size);

    EVP_MD_CTX_cleanup(rsaVerifyCtx);

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

RSA* Signature::createPublicRSA(fs::path publicKey)
{
    RSA* rsa = NULL;
    BIO* keyBio;

    std::vector<uint8_t> key ;
    getFileContent(publicKey, key);

    keyBio = BIO_new_mem_buf((void*)key.data(), -1);

    if (keyBio != NULL)
    {
        rsa = PEM_read_bio_RSA_PUBKEY(keyBio, &rsa, NULL, NULL);
    }

    return rsa;
}

void* Signature::mapFile(fs::path path, size_t size)
{
    int fd = open(path.c_str(), O_RDONLY);

    return mmap(NULL, size, PROT_READ,
                MAP_PRIVATE, fd, 0);
}

void Signature::getFileContent(fs::path path,
                               std::vector<uint8_t>& fileData)
{
    std::ifstream file(path);

    if (file)
    {
        // Copy the whole file content into the buffer.
        std::copy(std::istreambuf_iterator<char>(file),
                  std::istreambuf_iterator<char>(),
                  std::back_inserter(fileData));
    }
}

} //namespace image
} //namespace software
} //namespace phosphor
