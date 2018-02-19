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

using KeyType = std::string;
using HashType = std::string;
using PublicKeyPath = fs::path;
using HashFilePath = fs::path;
using KeyHashPathPair = std::pair<HashFilePath, PublicKeyPath>;
using AvailableKeyTypes = std::vector<KeyType>;

/** @class KeyManager
 *  @brief Class to manage hash and encrption key functionality
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

        KeyManager( const fs::path& extractPath):
                    _extractPath(extractPath),
                    _signedConfPath(SIGNED_IMAGE_CONF_PATH) {}

        //Passing paths for unit testing purpose
        KeyManager( const fs::path& extractPath,
                    const fs::path& signedConfPath):
                    _extractPath(extractPath),
                    _signedConfPath(signedConfPath) {}

        /**
         * @brief Read the hash algorithm type defined in the MANIFEST file
         *
         * @return HashType
         **/
        HashType getHashTypeFromManifest();

        /**
         * @brief Read the hash algorithm type defined in the specified file
         *
         * @param[in]  file - hash type file.
         *
         * @return HashType
         **/
        HashType getHashTypeFromFile(const HashFilePath& file);

        /**
         * @brief Read key type defined in the MANIFEST file
         *
         * @return KeyType
         **/
        KeyType getKeyTypeFromManifest();

        /**
         * @brief Save hash function details from MANIFEST file to the system
         *
         **/
        void saveHashTypeToSystem();

        /**
         * @brief Save public key file from image to the system
         *
         **/
        void savePublicKeyToSystem();

        /**
         * @brief Return all key types stored in the system
         *
         * @return list
         **/
        AvailableKeyTypes getAvailableKeyTypesFromSystem();

        /**
         * @brief Return publick key and hash function file names for the
         *        corresponding key type
         *
         * @param[in]  key - key type
         * @return Pair of hash and public key file names
         **/
        KeyHashPathPair getKeyHashFileNames(const KeyType& key);

    private:
        /**
         * @brief Path where the image tar file is extracted
         */
        fs::path _extractPath;


        /**
         * @brief Path of public key and hash function files
         */
        fs::path _signedConfPath;

};
} // namespace image
} // namespace software
} // namespace phosphor

