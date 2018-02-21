#pragma once
#include <experimental/filesystem>

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

//BMC flash image file name list.
const std::vector<std::string> bmcImages = { "image-kernel",
                                             "image-rofs",
                                             "image-rwfs",
                                             "image-u-boot"
                                           };
/** @class Signature
 *  @brief Contains signature verification functions.
 *  @details The software image class that contains the signature
 *           verification functions for signed image.
 */
class Signature
{
    public:
        Signature() = delete;
        Signature(const Signature&) = delete;
        Signature& operator=(const Signature&) = delete;
        Signature(Signature&&) = delete;
        Signature& operator=(Signature&&) = delete;
        ~Signature() = default;

        /** @brief Constructs Signature.
         * @param[in]  imageDirPath - image path
         * @param[in]  signedConfPath - Path of public key
         *                              hash function files
         */
        Signature(const fs::path& imageDirPath,
                  const fs::path& signedConfPath);

        /**
         * @brief Image signature verification function.
         *        Verify the Manifest and public key file signature using the
         *        public keys available in the system first. After successful
         *        validation, continue the whole image files signature
         *        validation using the image specific public key and the
         *        hash function.
         *
         *        @return true if signature verification was successful,
         *                     false if not
         */
        bool verify();

    private:
        /**
         * @brief Function used for system level file signature validation
         *        using the available public keys and hash functions
         *        in the system.
         *        @param[in]  name - file name
         */
        bool systemLevelVerify(const std::string& name);

        /**
         *  @brief Return all key types stored in the BMC based on the
         *         public key and hashfunc files stored in the BMC.
         *
         *  @return list
         */
        inline const AvailableKeyTypes getAvailableKeyTypesFromSystem();

        /**
         *  @brief Return public key and hash function file names for the
         *  corresponding key type
         *
         *  @param[in]  key - key type
         *  @return Pair of hash and public key file names
         */
        KeyHashPathPair getKeyHashFileNames(const Key_t& key);

        /**
         * @brief Read the hash algorithm type defined in the specified file
         *
         * @param[in]  file - hash type file.
         *
         * @return Hash type
         */
        inline const Hash_t getHashTypeFromFile(const HashFilePath& file);

        /**
         * @brief Verify the file signature using public key and hash function
         *
         * @param[in]  - Image file path
         * @param[in]  - Signature file path
         * @param[in]  - Public key
         * @param[in]  - Hash function name
         * @return true if signature verification was successful, false if not
         */
        bool verifyFile(const fs::path& file,
                        const fs::path& signature,
                        const fs::path& publicKey,
                        const std::string& hashFunc);


        /** @brief Directory where software images are placed */
        fs::path imageDirPath;

        /** @brief Path of public key and hash function files */
        fs::path signedConfPath;

        /** @brief Path of the public key file extracted from the image */
        fs::path publicKeyFile;

        /** @brief key type defined in mainfest file */
        Key_t keyType;

        /** @brief Hash type defined in mainfest file */
        Hash_t hashType;
};

} // namespace image
} // namespace software
} // namespace phosphor
