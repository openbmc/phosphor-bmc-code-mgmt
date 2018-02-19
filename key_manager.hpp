#pragma once
#include <string>
#include <vector>
namespace phosphor
{
namespace software
{
namespace manager
{
constexpr auto publicKeyFileName = "PublicKey.pem";
constexpr auto hashFileName = "HashType.hash";

using KeyTypeFile = std::string;
using Value = std::string;
using KeyType = std::string;
using HashType = std::string;
using HashFilePath = std::string;
using PublicKeyPath = std::string;
using KeyFileList = std::vector<KeyTypeFile>;
using HashFileList = std::vector<HashFilePath>;


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

        KeyManager( const std::string& manifestFile):
                    _manifestFile(manifestFile),
                    _hashFilesPath(hashFilesPath),
                    _keyFilesPath(publicKeysPath) {}

        KeyManager( const std::string& manifestFile,
                    const std::string& hashFilesPath,
                    const std::string& publicKeysPath):
                    _manifestFile(manifestFile),
                    _hashFilesPath(hashFilesPath),
                    _keyFilesPath(publicKeysPath) {}

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
         * @brief Save hash type value to the system
         *
         * Read hash type value from the manifest file and save in onto the
         * system
         *
         * @return None
         **/
        void saveHashTypeToSystem();

        /**
         * @brief Save the public key file to the system
         *
         * @param[in] srcFile source file path
         *
         * @return None
         **/
        void savePublicKeyToSystem(const std::string& srcFile);

        /**
         * @brief Return available public key files from the system
         *
         * @return list
         **/
        KeyFileList getAllKeyFilePath();

        /**
         * @brief Return available hash algorithm files from system
         *
         * @return list
         **/
        HashFileList getAllHashFilePath();

    private:
        /**
         * @brief Read the tag value from the file
         *
         * @param[in] file file to read
         * @param[in] keyTag tag value to read
         *
         * @return key value
         **/
        Value getValue(const std::string& file, const std::string& keyTag);

        /**
         * @brief Checks if the given string ends with given match
         * @param[in] mainStr main string
         * @param[in] toMatch match value
         * @return true if match else false
         */
        bool endsWith(const std::string &mainStr, const std::string &toMatch);

        std::string _manifestFile;
        std::string _hashFilesPath;
        std::string _keyFilesPath;
        static constexpr auto hashFilesPath = "/etc/activationdata/hash";
        static constexpr auto publicKeysPath = "/etc/activationdata/key";

};
} // namespace manager
} // namespace software
} // namespace phosphor

