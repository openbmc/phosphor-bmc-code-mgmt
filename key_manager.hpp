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
using AvailableKeys = std::vector<std::string>;

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
                    hashFile(systemHashFunctionFile),
                    keyTypeFile(systemKeyFile)

        {
        }

        HashType getHashTypeFromManifest();
        KeyType getKeyTypeFromManifest();

        HashType getHashTypeFromSystem();
        KeyType getKeyTypeFromSystem();

        void updatePublickKeyToSystem();
        void updateHashTypeToSystem();
        void updateKeyTypeToSystem();

        AvailableKeys getAllKeysFromSystem();

        bool isPublicKeyFoundInManifest();

    private:
        KeyManager( const ManiFestFile& manifest,
                    const HashFunctionFile& hash,
                    const KeyTypeFile& key):
                    manifestFile(manifest),
                    hashFile(hash),
                    keyTypeFile(key)

        {
        }

        Value getValue(std::fstream& stream, const std::string& keyTag);

        ManiFestFile manifestFile;
        HashFunctionFile hashFile;;
        KeyTypeFile keyTypeFile;
};
} // namespace manager
} // namespace software
} // namespace phosphor

