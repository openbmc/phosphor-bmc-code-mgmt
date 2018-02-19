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

enum class HashType {   md4, md5, sha, sha1, sha224, sha256,
                        sha384, sha512 };

enum class KeyType { OpenBMC, GA, GHE, Witherspoon };

using KeyTypeFile = std::string;
using Value = std::string;
using AvailKeysPath = std::vector<std::string>;

constexpr auto OpenBMC = "OpenBMC";
constexpr auto GA= "GA";
constexpr auto GHE= "GHE";
constexpr auto Witherspoon= "Witherspoon";
constexpr auto SystemHashFile = "/etc/activationdata/hash";
constexpr auto PublicKeysPath = "/etc/activationdata/keys";

class KeyManager
{
    public:
        KeyManager() = delete;
        KeyManager(const KeyManager&) = delete;
        KeyManager& operator=(const KeyManager&) = delete;
        KeyManager(KeyManager&&) = default;
        KeyManager& operator=(KeyManager&&) = default;
        ~KeyManager() = default;

        KeyManager( const std::string& i_manifestFile):
                    manifestFile(i_manifestFile),
                    systemHashFile(SystemHashFile),
                    publicKeysPath(PublicKeysPath){}

        KeyManager( const std::string& i_manifestFile,
                    const std::string& i_systemHashFile,
                    const std::string& i_publicKeysPath):
                    manifestFile(i_manifestFile),
                    systemHashFile(i_systemHashFile),
                    publicKeysPath(i_publicKeysPath){}

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
         * @brief Save hash type value to the system
         *
         * Read hash type value from the manifest file and update onto the
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
        void savePublicKeyToSystem(std::string& srcFile);

        /**
         * @brief Parse public keys present in the system and return the data
         *
         * @return List of public keys
         **/
        AvailKeysPath getAvailKeysPathFromSystem();

    private:
        /**
         * @brief Read the tag value from the configuration file
         *
         * @param[in] file configuration file
         * @param[in] keyTag tag value to read

         * @return key value
         **/
        Value getValue(const std::string& file, const std::string& keyTag);

        HashType getHashTypeEnum(const std::string& type);
        KeyType getKeyTypeEnum(const std::string& type);

        /**
         * @brief Checks if the given string ends with given match
         * @param[in] mainStr main string
         * @param[in] toMatch match value
         * @return true if match else false
         */
        bool endsWith(const std::string &mainStr, const std::string &toMatch);

        std::string manifestFile;
        std::string systemHashFile;
        std::string publicKeysPath;
};
} // namespace manager
} // namespace software
} // namespace phosphor

