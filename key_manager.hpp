#pragma once
#include <string>
#include <vector>
#include <map>
#include <utility>
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
using KeyHashPair = std::pair<HashFilePath, PublicKeyPath>;
using AllKeysAndHash = std::map<KeyType, KeyHashPair>;

constexpr auto OpenBMC = "OpenBMC";
constexpr auto GA = "GA";
constexpr auto GHE = "GHE";
constexpr auto Witherspoon = "Witherspoon";

constexpr auto md4 = "MD4";
constexpr auto md5 = "MD5";
constexpr auto sha = "SHA";
constexpr auto sha1 = "SHA1";
constexpr auto sha224 = "SHA224";
constexpr auto sha256 = "SHA256";
constexpr auto sha384 = "SHA384";
constexpr auto sha512 = "SHA512";

constexpr auto hashFilesPath = "/etc/activationdata/hash";
constexpr auto publicKeysPath = "/etc/activationdata/key";

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
                    _keyFilesPath(publicKeysPath)
        {
            init();
        }

        KeyManager( const std::string& manifestFile,
                    const std::string& hashFilesPath,
                    const std::string& publicKeysPath):
                    _manifestFile(manifestFile),
                    _hashFilesPath(hashFilesPath),
                    _keyFilesPath(publicKeysPath)
        {
            init();
        }

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
         * @brief Read the hash algorithm type defined in the system
         * Returns hash corresponding to the key type specified in manifest file
         *
         * @return HashType
         **/
        HashType getHashTypeFromSystem();


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
         * @brief Return available hash and public key file paths in the system
         *
         * @return list
         **/
        AllKeysAndHash getAllKeysAndHashFromSystem();

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

        PublicKeyPath getPublicKeyFilePath(const KeyType& type);
        HashFilePath getHashFilePath(const KeyType& type);

        void init();

        std::string _manifestFile;
        std::string _hashFilesPath;
        std::string _keyFilesPath;
        std::vector<KeyType> _keyTypesList;
};
} // namespace manager
} // namespace software
} // namespace phosphor

