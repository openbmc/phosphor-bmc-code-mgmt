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

/** @class KeyManager
 *  @brief Class to manage hash and secure image functionality
 */
class KeyManager
{
    public:
        KeyManager() = delete;
        KeyManager(const KeyManager&) = delete;
        KeyManager& operator=(const KeyManager&) = delete;
        KeyManager(KeyManager&&) = default;
        KeyManager& operator=(KeyManager&&) = default;
        ~KeyManager() = default;

        /**
         * @brief Constructor
         * @param[in]  extractPath - temp path where image is extracted
         **/
        KeyManager( const fs::path& extractPath):
                    extractPath(extractPath),
                    signedConfPath(SIGNED_IMAGE_CONF_PATH) {}

        /**
         * @brief Constructor
         * @param[in]  extractPath - temp path where image is extracted
         * @param[in]  signedConfPath - path where hash file and public key
                                        is stored.
         **/
        KeyManager( const fs::path& extractPath,
                    const fs::path& signedConfPath):
                    extractPath(extractPath),
                    signedConfPath(signedConfPath) {}

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
         * @brief Save hash function details from MANIFEST file to the system
         *
         * @return true on success and false on failure
         **/
        bool saveHashTypeToSystem();

        /**
         * @brief Save public key file from image to the system
         *
         * @return true on sucess and fale on failure
         **/
        bool savePublicKeyToSystem();

        /**
         * @brief Return all key types stored in the system based on the
         *        public key and hashfunc files stored in the system.
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
         * @brief Path where the image tar file is extracted
         */
        fs::path extractPath;


        /**
         * @brief Path of public key and hash function files
         */
        fs::path signedConfPath;

};
} // namespace image
} // namespace software
} // namespace phosphor

