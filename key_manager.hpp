#pragma once
#include <string>
#include <vector>
#include <experimental/filesystem>
#include "config.h"

namespace phosphor
{
namespace software
{
namespace manager
{
namespace fs = std::experimental::filesystem;

using KeyType = std::string;
using HashType = std::string;
using PublicKeyPath = fs::path;
using PubKeyPathList = std::vector<fs::path>;
using HashFilePathList = std::vector<fs::path>;


/** @class Version
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
                    _hashFilesPath(HASH_FILE_PATH),
                    _pubKeyFilesPath(PUB_KEY_FILE_PATH) {}

        //Passing paths for unit testing purpose
        KeyManager( const fs::path& extractPath,
                    const fs::path& hashFilesPath,
                    const fs::path& publicKeysPath):
                    _extractPath(extractPath),
                    _hashFilesPath(hashFilesPath),
                    _pubKeyFilesPath(publicKeysPath) {}

        /**
         * @brief Read the hash algorithm type defined in the MANIFEST file
         *
         * @return HashType
         **/
        HashType getHashTypeFromManifest();

        /**
         * @brief Read key type defined in the MANIFEST file
         *
         * @return KeyType
         **/
        KeyType getKeyTypeFromManifest();

        /**
         * @brief Write the hash type from manifest to a file in the system
         *
         *
         * @return None
         **/
        void saveHashTypeToSystem();

        /**
         * @brief Save public key file from the extract path to the system
         *
         * @return None
         **/
        void savePublicKeyToSystem();

        /**
         * @brief Return available public key files from the system
         *
         * @return list
         **/
        PubKeyPathList getAllPubKeyFilePath();

        /**
         * @brief Return available hash algorithm files from the system
         *
         * @return list
         **/
        HashFilePathList getAllHashFilePath();

    private:

        /**
         * @brief Path where the image tar file is extracted
         */
        fs::path _extractPath;

        /**
         * @brief Path where hash files will be placed
         */
        fs::path _hashFilesPath;

        /**
         * @brief Path where public key files will be placed
         */
        fs::path _pubKeyFilesPath;

};
} // namespace manager
} // namespace software
} // namespace phosphor

