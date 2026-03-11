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
#include <iostream>
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
constexpr auto hashTagSuffix = "_Hash_Type";
constexpr auto EVP_PKEY_MLDSA87 = 6;

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
    pqAlgorithm = getPQAlgorithmFromManifest();
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
            if (key.parent_path() == signedConfPath)
            {
                keyTypes.insert(key.filename());
            }
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

std::optional<Signature::PQAlgorithm> Signature::getPQAlgorithmFromManifest()
    const
{
    fs::path manifestFile(imageDirPath / MANIFEST_FILE_NAME);

    std::ifstream efile(manifestFile);
    if (!efile.good())
    {
        return std::nullopt;
    }

    std::string line;
    while (std::getline(efile, line))
    {
        // Look for lines ending with "_Hash_Type"
        auto delimPos = line.find('=');
        if (delimPos == std::string::npos)
        {
            continue;
        }

        std::string key = line.substr(0, delimPos);
        std::string value = line.substr(delimPos + 1);

        if (key.find(hashTagSuffix) == std::string::npos)
        {
            continue;
        }

        auto commaPos = value.find(',');
        if (commaPos == std::string::npos)
        {
            continue;
        }

        Signature::PQAlgorithm algo;
        algo.hashType = value.substr(0, commaPos);
        algo.name = value.substr(commaPos + 1);

        return algo;
    }

    return std::nullopt;
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

    if (ret)
    {
        if (pqAlgorithm.has_value())
        {
            std::error_code ec2;
            fs::path algoDir(imageDirPath / pqAlgorithm->name);

            if (fs::exists(algoDir, ec2))
            {
                fs::path algoFullImageSig = algoDir / imageFullSig;
                fs::path algoPublicKeyFile = algoDir / PUBLICKEY_FILE_NAME;

                if (fs::exists(algoFullImageSig, ec2) &&
                    fs::exists(tmpFullFile, ec2))
                {
                    ret = verifyFile(tmpFullFile, algoFullImageSig,
                                     algoPublicKeyFile, pqAlgorithm->hashType);
                    if (!ret)
                    {
                        error(
                            "Full image signature validation failed for {ALGO}",
                            "ALGO", pqAlgorithm->name);
                    }
                }
            }
        }
    }

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
        if (!systemLevelVerify())
        {
            error("System level Signature Validation failed");
            std::cout << "systemLevelVerify failed" << std::endl;
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
                                    imageUpdateList, bmcFilesFound, hashType);
        if (bmcFilesFound && !valid)
        {
            std::cout << "checkAndVerifyImage failed" << std::endl;
            return false;
        }

        if (!valid)
        {
            // Validate bmcImages
            imageUpdateList.clear();
            imageUpdateList.assign(bmcImages.begin(), bmcImages.end());
            valid =
                checkAndVerifyImage(imageDirPath, publicKeyFile,
                                    imageUpdateList, bmcFilesFound, hashType);

            if (bmcFilesFound && valid)
            {
                if (!verifyPQSignatures(bmcImages))
                {
                    std::cout << "verifyPQSignatures failed" << std::endl;
                    error("PQ image signature verification failed");
                    return false;
                }
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
                optionalImagesValid =
                    verifyFile(file, sigFile, publicKeyFile, hashType);
                if (!optionalImagesValid)
                {
                    error("Image file Signature Validation failed on {IMAGE}",
                          "IMAGE", optionalImage);
                    return false;
                }

                if (pqAlgorithm.has_value())
                {
                    fs::path algoDir(imageDirPath / pqAlgorithm->name);

                    if (fs::exists(algoDir, ec))
                    {
                        fs::path algoPublicKeyFile =
                            algoDir / PUBLICKEY_FILE_NAME;
                        fs::path algoSigFile =
                            algoDir / (optionalImage + ".sig");

                        if (fs::exists(algoSigFile, ec))
                        {
                            bool algoValid =
                                verifyFile(file, algoSigFile, algoPublicKeyFile,
                                           pqAlgorithm->hashType);
                            if (!algoValid)
                            {
                                error(
                                    "Signature validation failed for optional image {IMAGE} with {ALGO}",
                                    "IMAGE", optionalImage, "ALGO",
                                    pqAlgorithm->name);
                                return false;
                            }
                        }
                    }
                }
            }
        }

        if (!verifyFullImage())
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

        debug("Successfully completed Signature validation.");
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
                    if (pqAlgorithm.has_value())
                    {
                        std::error_code ec;
                        fs::path algoDir(imageDirPath / pqAlgorithm->name);

                        if (!fs::exists(algoDir, ec))
                        {
                            break; // RSA passed, PQ optional
                        }

                        // Check if system has corresponding key
                        fs::path systemAlgoKeyPath =
                            signedConfPath / keyType / pqAlgorithm->name /
                            PUBLICKEY_FILE_NAME;

                        if (!fs::exists(systemAlgoKeyPath, ec))
                        {
                            break; // RSA passed, PQ optional
                        }

                        fs::path algoManifestSig = algoDir / MANIFEST_FILE_NAME;
                        algoManifestSig.replace_extension(SIGNATURE_FILE_EXT);

                        fs::path algoPkeySig = algoDir / PUBLICKEY_FILE_NAME;
                        algoPkeySig.replace_extension(SIGNATURE_FILE_EXT);

                        valid = verifyFile(manifestFile, algoManifestSig,
                                           systemAlgoKeyPath,
                                           pqAlgorithm->hashType);
                        if (!valid)
                        {
                            error("System level verification failed for {ALGO}",
                                  "ALGO", pqAlgorithm->name);
                            break;
                        }

                        valid =
                            verifyFile(pkeyFile, algoPkeySig, systemAlgoKeyPath,
                                       pqAlgorithm->hashType);
                        if (!valid)
                        {
                            error(
                                "System level publickey verification failed for {ALGO}",
                                "ALGO", pqAlgorithm->name);
                            break;
                        }
                    }

                    if (valid)
                    {
                        break;
                    }
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
    std::cout << "inside verifyFile" << std::endl;
    std::cout << "file: " << file << std::endl;
    std::cout << "sigFile: " << sigFile << std::endl;
    std::cout << "publicKey: " << publicKey << std::endl;
    std::cout << "hashFunc: " << hashFunc << std::endl;

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

    auto publicKeyPtr = createPublicKey(publicKey);
    if (!publicKeyPtr)
    {
        error("Failed to create public key from {PATH}", "PATH", publicKey);
        elog<InternalFailure>();
    }

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

    auto size = fs::file_size(file, ec);
    auto dataPtr = mapFile(file, size);

    auto sigSize = fs::file_size(sigFile, ec);
    auto signature = mapFile(sigFile, sigSize);

    // Check if this is an ML-DSA key
    int keyType = EVP_PKEY_id(publicKeyPtr.get());
    bool isMLDSA = (keyType == EVP_PKEY_MLDSA87);

    if (!isMLDSA)
    {
        // Initializes a digest context.
        EVP_MD_CTX_Ptr verifyCtx(EVP_MD_CTX_new(), ::EVP_MD_CTX_free);

        auto result = EVP_DigestVerifyInit(verifyCtx.get(), nullptr, hashStruct,
                                           nullptr, publicKeyPtr.get());

        if (result <= 0)
        {
            std::cout << "EVP_DigestVerifyInit failed" << std::endl;
            error("Error ({RC}) occurred during EVP_DigestVerifyInit", "RC",
                  ERR_get_error());
            elog<InternalFailure>();
        }

        // Hash the data file and update the verification context
        result = EVP_DigestVerifyUpdate(verifyCtx.get(), dataPtr(), size);
        if (result <= 0)
        {
            std::cout << "EVP_DigestVerifyUpdate failed" << std::endl;
            error("Error ({RC}) occurred during EVP_DigestVerifyUpdate", "RC",
                  ERR_get_error());
            elog<InternalFailure>();
        }

        // Verify the data with signature.
        result = EVP_DigestVerifyFinal(
            verifyCtx.get(), reinterpret_cast<unsigned char*>(signature()),
            sigSize);

        // Check the verification result.
        if (result < 0)
        {
            std::cout << "EVP_DigestVerifyFinal failed" << std::endl;
            error("Error ({RC}) occurred during EVP_DigestVerifyFinal", "RC",
                  ERR_get_error());
            elog<InternalFailure>();
        }

        if (result == 0)
        {
            std::cout << "Signature validationl failed" << std::endl;
            error("EVP_DigestVerifyFinal:Signature validation failed on {PATH}",
                  "PATH", sigFile);
            return false;
        }
        return true;
    }
    else
    {
        EVP_MD_CTX_Ptr hashCtx(EVP_MD_CTX_new(), ::EVP_MD_CTX_free);

        if (EVP_DigestInit_ex(hashCtx.get(), hashStruct, nullptr) != 1)
        {
            error("Error ({RC}) occurred during EVP_DigestInit_ex", "RC",
                  ERR_get_error());
            elog<InternalFailure>();
        }

        if (EVP_DigestUpdate(hashCtx.get(), dataPtr(), size) != 1)
        {
            error("Error ({RC}) occurred during EVP_DigestUpdate", "RC",
                  ERR_get_error());
            elog<InternalFailure>();
        }

        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hashLen = 0;

        if (EVP_DigestFinal_ex(hashCtx.get(), hash, &hashLen) != 1)
        {
            error("Error ({RC}) occurred during EVP_DigestFinal_ex", "RC",
                  ERR_get_error());
            elog<InternalFailure>();
        }

        EVP_MD_CTX_Ptr verifyCtx(EVP_MD_CTX_new(), ::EVP_MD_CTX_free);
        auto result = EVP_DigestVerifyInit(verifyCtx.get(), nullptr, nullptr,
                                           nullptr, publicKeyPtr.get());
        if (result <= 0)
        {
            error("Error ({RC}) occurred during EVP_DigestVerifyInit", "RC",
                  ERR_get_error());
            elog<InternalFailure>();
        }

        result = EVP_DigestVerify(verifyCtx.get(),
                                  reinterpret_cast<unsigned char*>(signature()),
                                  sigSize, hash, hashLen);

        if (result < 0)
        {
            error("Error ({RC}) occurred during EVP_DigestVerify", "RC",
                  ERR_get_error());
            elog<InternalFailure>();
        }

        if (result == 0)
        {
            error(
                "EVP_DigestVerify: ML-DSA signature validation failed on {PATH}",
                "PATH", sigFile);
            return false;
        }

        return true;
    }
}

inline EVP_PKEY_Ptr Signature::createPublicKey(const fs::path& publicKey)
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

bool Signature::checkAndVerifyImage(
    const std::string& filePath, const std::string& publicKeyPath,
    const std::vector<std::string>& imageList, bool& fileFound,
    const std::string& hashType, const std::string& sigSubDir)
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

        fs::path sigFile;
        if (sigSubDir.empty())
        {
            // Top-level: signature in same directory
            sigFile = file;
            sigFile += SIGNATURE_FILE_EXT;
        }
        else
        {
            // Subdirectory: signature in algorithm-specific subdirectory
            sigFile = fs::path(filePath) / sigSubDir /
                      (bmcImage + SIGNATURE_FILE_EXT);
        }

        // Verify the signature.
        valid = verifyFile(file, sigFile, publicKeyPath, hashType);
        if (!valid)
        {
            error("Image file Signature Validation failed on {PATH}", "PATH",
                  bmcImage);
            return false;
        }
    }

    return valid;
}

bool Signature::verifyPQSignatures(const std::vector<std::string>& imageList)
{
    if (!pqAlgorithm.has_value())
    {
        return true; // No PQ algorithm, skip
    }

    std::error_code ec;
    fs::path algoDir(imageDirPath / pqAlgorithm->name);

    if (!fs::exists(algoDir, ec))
    {
        return true; // PQ directory doesn't exist, optional
    }

    fs::path algoPublicKeyFile = algoDir / PUBLICKEY_FILE_NAME;
    if (!fs::exists(algoPublicKeyFile, ec))
    {
        error("Public key not found in {ALGO} directory", "ALGO",
              pqAlgorithm->name);
        return false;
    }

    bool algoFileFound = false;
    return checkAndVerifyImage(imageDirPath, algoPublicKeyFile.string(),
                               imageList, algoFileFound, pqAlgorithm->hashType,
                               pqAlgorithm->name);
}

} // namespace image
} // namespace software
} // namespace phosphor
