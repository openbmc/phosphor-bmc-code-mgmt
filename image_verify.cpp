#include <set>
#include <fstream>
#include <sys/stat.h>
#include <fcntl.h>

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

    std::string key{};
    std::size_t pos{};

    for (const auto& p : fs::recursive_directory_iterator(signedConfPath))
    {
        if ((p.path().filename() == HASH_FILE_NAME) ||
            (p.path().filename() == PUBLICKEY_FILE_NAME))
        {
            // extract the key types
            // /etc/activationdata/OpenBMC/  -> get OpenBMC from the path
            key = p.path().parent_path();
            pos = key.find_last_of("/\\");
            if ((key.length() > 0) && (pos != key.length() - 1))
            {
                keyTypes.emplace_back(key.substr(pos + 1));
            }
        }
    }

    return keyTypes;
}

inline const KeyHashPathPair Signature::getKeyHashFileNames(const Key_t& key)
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
            auto valid = verifyFile(file,
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
    catch (const InternalFailure& e)
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

    auto valid = false;

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

    //Check existence of the files in the system.
    if (!(fs::exists(file) && fs::exists(sigFile)))
    {
        log<level::ERR>("Failed to find the Data or signature file.",
                        entry("File=%s", file.c_str()));
        elog<InternalFailure>();
    }

    //Create RSA.
    auto publicRSA = createPublicRSA(publicKey);
    if (publicRSA.get() == NULL)
    {
        log<level::ERR>("Failed to create RSA",
                        entry("File=%s", publicKey.c_str()));
        elog<InternalFailure>();
    }

    //Assign public key to RSA.
    EVP_PKEY* pubKey  = EVP_PKEY_new();
    EVP_PKEY_assign_RSA(pubKey, publicRSA.get());

    //Initializes a digest context.
    EVP_MD_CTX_Ptr rsaVerifyCtx(EVP_MD_CTX_create(), ::EVP_MD_CTX_destroy);

    //Adds all digest algorithms to the internal table
    OpenSSL_add_all_digests();

    //Create  Hash structre.
    auto hashPtr = EVP_get_digestbyname(hashFunc.c_str());
    if (!hashPtr)
    {
        log<level::ERR>("EVP_get_digestbynam: Unknown message digest",
                        entry("HASH=%s", hashFunc.c_str()));
        elog<InternalFailure>();
    }

    auto result = EVP_DigestVerifyInit(rsaVerifyCtx.get(), NULL,
                                       hashPtr, NULL, pubKey);

    if (result <= 0)
    {
        log<level::ERR>("Error occured during EVP_DigestVerifyInit",
                        entry("ERRNO=%d", result));
        elog<InternalFailure>();
    }

    //Hash the data file and update the verification context
    auto size = fs::file_size(file);
    auto dataPtr = mapFile(file, size);

    result = EVP_DigestVerifyUpdate(rsaVerifyCtx.get(), dataPtr(), size);
    if (result <= 0)
    {
        log<level::ERR>("Error occured during EVP_DigestVerifyUpdate",
                        entry("ERRNO=%d", result));
        elog<InternalFailure>();
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
        elog<InternalFailure>();
    }

    if (result == 0)
    {
        log<level::ERR>("EVP_DigestVerifyFinal:Signature validation failed",
                        entry("path=%s", sigFile.c_str()));
        return false;
    }
    return true;
}

inline RSA_Ptr Signature::createPublicRSA(const fs::path& publicKey)
{
    RSA* rsa = nullptr;
    auto size = fs::file_size(publicKey);

    //Read public key file
    auto data = mapFile(publicKey, size);

    BIO_MEM_Ptr keyBio(BIO_new_mem_buf(data(), -1), &::BIO_free);
    if (keyBio.get() == NULL)
    {
        log<level::ERR>("Failed to create new BIO Memory buffer");
        elog<InternalFailure>();
    }

    RSA_Ptr rsaPtr(PEM_read_bio_RSA_PUBKEY(keyBio.get(), &rsa, NULL, NULL),
                   ::RSA_free);
    return rsaPtr;
}

CustomMap Signature::mapFile(const fs::path& path, size_t size)
{

    CustomFd fd(open(path.c_str(), O_RDONLY));

    return CustomMap(mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd(), 0), size);
}

} //namespace image
} //namespace software
} //namespace phosphor
