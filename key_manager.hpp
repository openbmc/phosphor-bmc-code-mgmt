#pragma once
#include <string>
#include <vector>
namespace phosphor
{
namespace software
{
namespace manager
{
constexpr auto publicKeyFileName = "publicKey.pem";
constexpr auto systemHashFunctionFile = "/etc/activationdata/HashFunction";
constexpr auto systemKeyFile = "/etc/activationdata/Keys";

enum class HashType {   md4, md5, ripemd160, sha, sha1, sha224, sha256,
                        sha384, sha512 };

enum class KeyType { OpenBMC, GA, GHE, Witherspoon };

using ManiFestFile = std::string;
using HashFunctionFile = std::string;
using KeyTypeFile = std::string;
using Value = std::string;
using AvailableKeys = std::vector<std::vector<uint8_t>>;

class KeyManager
{
    public:
        KeyManager() = delete;
        KeyManager(const KeyManager&) = delete;
        KeyManager& operator=(const KeyManager&) = delete;
        KeyManager(KeyManager&&) = default;
        KeyManager& operator=(KeyManager&&) = default;
        ~KeyManager() = default;

        KeyManager( const ManiFestFile& manifest):
                    manifestFile(manifest),
                    hashFile(systemHashFunctionFile)

        {
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
         *
         * @return HashType
         **/
        HashType getHashTypeFromSystem();

        /**
         * @brief Update public key into the system
         * Read public key datails if found and update the corresponding
         * key file in the system.
         *
         * @return None
         **/
        void updatePublickKeyToSystem();

        /**
         * @brief Update has type into the system config file
         * Read hash type used datails from the system and update onto the
         * system config file.
         *
         * @return None
         **/
        void updateHashTypeToSystem();

        /**
         * @brief Update public key into the ssytem
         * Read public key datails if found and update the corresponding
         * key file in the system.
         *
         * @return None
         **/
        void updateKeyTypeToSystem();

        /**
         * @brief Parse public keys present in the system and return the data
         *
         * @return List of public keys
         **/
        AvailableKeys getAllKeysFromSystem();

        bool isPublicKeyFoundInManifest();

    private:
        KeyManager( const ManiFestFile& manifest,
                    const HashFunctionFile& hash):
                    manifestFile(manifest),
                    hashFile(hash)

        {
        }

        Value getValue(const std::string& file, const std::string& keyTag);

        ManiFestFile manifestFile;
        HashFunctionFile hashFile;;
};
} // namespace manager
} // namespace software
} // namespace phosphor

