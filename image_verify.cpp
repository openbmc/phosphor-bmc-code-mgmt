#include "config.h"

#include "image_verify.hpp"

#include "images.hpp"
#include "utils.hpp"
#include "version.hpp"

#include <fcntl.h>
#include <openssl/err.h>
#include <sys/stat.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cassert>
#include <fstream>
#include <set>
#include <system_error>

namespace phosphor
{
namespace software
{
namespace image
{

PHOSPHOR_LOG2_USING;
using namespace phosphor::logging;
using namespace phosphor::software::manager;
using InternalFailure =
    sdbusplus::error::xyz::openbmc_project::common::InternalFailure;

constexpr auto keyTypeTag = "KeyType";
constexpr auto hashFunctionTag = "HashType";

Signature::Signature(const fs::path& imageDirPath,
                     const fs::path& signedConfPath) :
    imageDirPath(imageDirPath), signedConfPath(signedConfPath)
{
    fs::path file(imageDirPath / MANIFEST_FILE_NAME);

    keyType = Version::getValue(file, keyTypeTag);
    hashType = Version::getValue(file, hashFunctionTag);

    // Get purpose
    auto purposeString = Version::getValue(file, "purpose");
    auto convertedPurpose =
        sdbusplus::message::convert_from_string<VersionPurpose>(purposeString);
    purpose = convertedPurpose.value_or(Version::VersionPurpose::Unknown);
}

AvailableKeyTypes Signature::getAvailableKeyTypesFromSystem() const
{
    AvailableKeyTypes keyTypes{};

    // Find the path of all the files
    std::error_code ec;
    if (!fs::is_directory(signedConfPath, ec))
    {
        error("Signed configuration path not found in the system: {ERROR_MSG}",
              "ERROR_MSG", ec.message());
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

bool Signature::verifyFullImage()
{
    bool ret = true;
#ifdef WANT_SIGNATURE_VERIFY
    // Only verify full image for BMC
    if (purpose != VersionPurpose::BMC)
    {
        return ret;
    }

    std::vector<std::string> fullImages = {
        fs::path(imageDirPath) / "image-bmc.sig",
        fs::path(imageDirPath) / "image-hostfw.sig",
        fs::path(imageDirPath) / "image-kernel.sig",
        fs::path(imageDirPath) / "image-rofs.sig",
        fs::path(imageDirPath) / "image-rwfs.sig",
        fs::path(imageDirPath) / "image-u-boot.sig",
        fs::path(imageDirPath) / "MANIFEST.sig",
        fs::path(imageDirPath) / "publickey.sig"};

    // Merge files
    std::string tmpFullFile = "/tmp/image-full";
    utils::mergeFiles(fullImages, tmpFullFile);

    // Validate the full image files
    fs::path pkeyFullFile(tmpFullFile);

    std::string imageFullSig = "image-full.sig";
    fs::path pkeyFullFileSig(imageDirPath / imageFullSig);
    pkeyFullFileSig.replace_extension(SIGNATURE_FILE_EXT);

    // image specific publickey file name.
    fs::path publicKeyFile(imageDirPath / PUBLICKEY_FILE_NAME);

    ret = verifyFile(pkeyFullFile, pkeyFullFileSig, publicKeyFile, hashType);

    std::error_code ec;
    fs::remove(tmpFullFile, ec);
#endif

    return ret;
}

bool Signature::verify()
{
    try
    {
        bool valid;
        // Verify the MANIFEST and publickey file using available
        // public keys and hash on the system.
        if (false == systemLevelVerify())
        {
            error("System level Signature Validation failed");
            return false;
        }

        bool bmcFilesFound = false;
        // image specific publickey file name.
        fs::path publicKeyFile(imageDirPath / PUBLICKEY_FILE_NAME);

        // Record the images which are being updated
        // First check and Validate for the fullimage, then check and Validate
        // for images with partitions
        std::vector<std::string> imageUpdateList = {bmcFullImages};
        valid = checkAndVerifyImage(imageDirPath, publicKeyFile,
                                    imageUpdateList, bmcFilesFound);
        if (bmcFilesFound && !valid)
        {
            return false;
        }

        if (!valid)
        {
            // Validate bmcImages
            imageUpdateList.clear();
            imageUpdateList.assign(bmcImages.begin(), bmcImages.end());
            valid = checkAndVerifyImage(imageDirPath, publicKeyFile,
                                        imageUpdateList, bmcFilesFound);
            if (bmcFilesFound && !valid)
            {
                return false;
            }
        }

        // Validate the optional image files.
        auto optionalImages = getOptionalImages();
        bool optionalFilesFound = false;
        bool optionalImagesValid = false;
        for (const auto& optionalImage : optionalImages)
        {
            // Build Image File name
            fs::path file(imageDirPath);
            file /= optionalImage;

            std::error_code ec;
            if (fs::exists(file, ec))
            {
                optionalFilesFound = true;
                // Build Signature File name
                fs::path sigFile(file);
                sigFile += SIGNATURE_FILE_EXT;

                // Verify the signature.
                optionalImagesValid = verifyFile(file, sigFile, publicKeyFile,
                                                 hashType);
                if (!optionalImagesValid)
                {
                    error("Image file Signature Validation failed on {IMAGE}",
                          "IMAGE", optionalImage);
                    return false;
                }
            }
        }

        if (verifyFullImage() == false)
        {
            error("Image full file Signature Validation failed");
            return false;
        }

        if (!bmcFilesFound && !optionalFilesFound)
        {
            error("Unable to find files to verify");
            return false;
        }

        // Either BMC images or optional images shall be valid
        assert(valid || optionalImagesValid);

        debug("Successfully completed Signature vaildation.");
        return true;
    }
    catch (const InternalFailure& e)
    {
        return false;
    }
    catch (const std::exception& e)
    {
        error("Error during processing: {ERROR}", "ERROR", e);
        return false;
    }
}

bool Signature::systemLevelVerify()
{
    // Get available key types from the system.
    auto keyTypes = getAvailableKeyTypesFromSystem();
    if (keyTypes.empty())
    {
        error("Missing Signature configuration data in system");
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
    // For any internal failure during the key/hash pair specific
    // validation, should continue the validation with next
    // available Key/hash pair.
    for (const auto& keyType : keyTypes)
    {
        auto keyHashPair = getKeyHashFileNames(keyType);

        auto hashFunc = Version::getValue(keyHashPair.first, hashFunctionTag);

        try
        {
            // Verify manifest file signature
            valid = verifyFile(manifestFile, manifestFileSig,
                               keyHashPair.second, hashFunc);
            if (valid)
            {
                // Verify publickey file signature.
                valid = verifyFile(pkeyFile, pkeyFileSig, keyHashPair.second,
                                   hashFunc);
                if (valid)
                {
                    break;
                }
            }
        }
        catch (const InternalFailure& e)
        {
            valid = false;
        }
    }
    return valid;
}

bool Signature::verifyFile(const fs::path& file, const fs::path& sigFile,
                           const fs::path& publicKey,
                           const std::string& hashFunc)
{
    // Check existence of the files in the system.
    std::error_code ec;
    if (!(fs::exists(file, ec) && fs::exists(sigFile, ec)))
    {
        error("Failed to find the Data or signature file {PATH}", "PATH", file);
        if (ec)
        {
            error("Error message: {ERROR_MSG}", "ERROR_MSG", ec.message());
        }
        elog<InternalFailure>();
    }

    // Create RSA.
    auto publicRSA = createPublicRSA(publicKey);
    if (!publicRSA)
    {
        error("Failed to create RSA from {PATH}", "PATH", publicKey);
        elog<InternalFailure>();
    }

    // Initializes a digest context.
    EVP_MD_CTX_Ptr rsaVerifyCtx(EVP_MD_CTX_new(), ::EVP_MD_CTX_free);

    // Adds all digest algorithms to the internal table
    OpenSSL_add_all_digests();

    // Create Hash structure.
    auto hashStruct = EVP_get_digestbyname(hashFunc.c_str());
    if (!hashStruct)
    {
        error("EVP_get_digestbynam: Unknown message digest: {HASH}", "HASH",
              hashFunc);
        elog<InternalFailure>();
    }

    auto result = EVP_DigestVerifyInit(rsaVerifyCtx.get(), nullptr, hashStruct,
                                       nullptr, publicRSA.get());

    if (result <= 0)
    {
        error("Error ({RC}) occurred during EVP_DigestVerifyInit", "RC",
              ERR_get_error());
        elog<InternalFailure>();
    }

    // Hash the data file and update the verification context
    auto size = fs::file_size(file, ec);
    auto dataPtr = mapFile(file, size);

    result = EVP_DigestVerifyUpdate(rsaVerifyCtx.get(), dataPtr(), size);
    if (result <= 0)
    {
        error("Error ({RC}) occurred during EVP_DigestVerifyUpdate", "RC",
              ERR_get_error());
        elog<InternalFailure>();
    }

    // Verify the data with signature.
    size = fs::file_size(sigFile, ec);
    auto signature = mapFile(sigFile, size);

    result = EVP_DigestVerifyFinal(
        rsaVerifyCtx.get(), reinterpret_cast<unsigned char*>(signature()),
        size);

    // Check the verification result.
    if (result < 0)
    {
        error("Error ({RC}) occurred during EVP_DigestVerifyFinal", "RC",
              ERR_get_error());
        elog<InternalFailure>();
    }

    if (result == 0)
    {
        error("EVP_DigestVerifyFinal:Signature validation failed on {PATH}",
              "PATH", sigFile);
        return false;
    }
    return true;
}

inline EVP_PKEY_Ptr Signature::createPublicRSA(const fs::path& publicKey)
{
    std::error_code ec;
    auto size = fs::file_size(publicKey, ec);

    // Read public key file
    auto data = mapFile(publicKey, size);

    BIO_MEM_Ptr keyBio(BIO_new_mem_buf(data(), -1), &::BIO_free);
    if (keyBio.get() == nullptr)
    {
        error("Failed to create new BIO Memory buffer");
        elog<InternalFailure>();
    }

    return {PEM_read_bio_PUBKEY(keyBio.get(), nullptr, nullptr, nullptr),
            &::EVP_PKEY_free};
}

CustomMap Signature::mapFile(const fs::path& path, size_t size)
{
    CustomFd fd(open(path.c_str(), O_RDONLY));

    return CustomMap(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd(), 0),
                     size);
}

bool Signature::checkAndVerifyImage(const std::string& filePath,
                                    const std::string& publicKeyPath,
                                    const std::vector<std::string>& imageList,
                                    bool& fileFound)
{
    bool valid = true;

    fileFound = false;
    for (auto& bmcImage : imageList)
    {
        fs::path file(filePath);
        file /= bmcImage;

        std::error_code ec;
        if (!fs::exists(file, ec))
        {
            valid = false;
            break;
        }
        fileFound = true;

        fs::path sigFile(file);
        sigFile += SIGNATURE_FILE_EXT;

        // Verify the signature.
        valid = verifyFile(file, sigFile, publicKeyPath, hashType);
        if (valid == false)
        {
            error("Image file Signature Validation failed on {PATH}", "PATH",
                  bmcImage);
            return false;
        }
    }

    return valid;
}
} // namespace image
} // namespace software
} // namespace phosphor
