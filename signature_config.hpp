#pragma once
#include <string>
#include <vector>
#include <experimental/filesystem>
#include <utility>
#include "config.h"

namespace phosphor
{
namespace software
{
namespace image
{
namespace fs = std::experimental::filesystem;

using Key_t = std::string;
using Hash_t = std::string;
using PublicKeyPath = fs::path;
using HashFilePath = fs::path;
using KeyHashPathPair = std::pair<HashFilePath, PublicKeyPath>;
using AvailableKeyTypes = std::vector<Key_t>;

/** @class SignatureConfig
 *  @brief Class to manage hash and secure image functionality
 */
class SignatureConfig
{
    public:
        SignatureConfig() = delete;
        SignatureConfig(const SignatureConfig&) = delete;
        SignatureConfig& operator=(const SignatureConfig&) = delete;
        SignatureConfig(SignatureConfig&&) = default;
        SignatureConfig& operator=(SignatureConfig&&) = default;
        ~SignatureConfig() = default;

        /**
         * @brief Constructor
         * @param[in]  extractPath - temp path where image is extracted
         * @param[in]  confPath - path where hash file and public key
                                        is stored.
         **/
        SignatureConfig(const fs::path& extractPath,
                        const fs::path& confPath);

        /**
         * @brief Read the hash algorithm type defined in the MANIFEST file
         *
         * @return Hash type
         **/
        Hash_t getHashTypeFromManifest();

        /**
         * @brief Read the hash algorithm type defined in the specified file
         *
         * @param[in]  file - hash type file.
         *
         * @return Hash type
         **/
        Hash_t getHashTypeFromFile(const HashFilePath& file);

        /**
         * @brief Read key type defined in the MANIFEST file
         *
         * @return key type
         **/
        Key_t getKeyTypeFromManifest();

        /**
         * @brief Save hash function details from MANIFEST file to the BMC
         *
         * @return true on success and false on failure
         **/
        bool saveHashTypeToSystem();

        /**
         * @brief Save public key file from image to the BMC
         *
         * @return true on success and false on failure
         **/
        bool savePublicKeyToSystem();

        /**
         * @brief Return all key types stored in the BMC based on the
         *        public key and hashfunc files stored in the BMC.
         *
         * @return list
         **/
        AvailableKeyTypes getAvailableKeyTypesFromSystem();

        /**
         * @brief Return public key and hash function file names for the
         *        corresponding key type
         *
         * @param[in]  key - key type
         * @return Pair of hash and public key file names
         **/
        KeyHashPathPair getKeyHashFileNames(const Key_t& key);

    private:

        /**
         * @brief Path of the public key file extracted from the image
         */
        fs::path publicKeyFile;

        /**
         * @brief Hash type defined in mainfest file
         */
        Key_t keyType;

        /**
         * @brief Key type defined in mainfest file
         */
        Hash_t hashType;

        /**
         * @brief Path of public key and hash function files
         */
        fs::path signedConfPath;

};
} // namespace image
} // namespace software
} // namespace phosphor

