#pragma once
#include <openssl/rsa.h>
#include <experimental/filesystem>

namespace phosphor
{
namespace software
{
namespace image
{

namespace fs = std::experimental::filesystem;

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
        Signature(const Signature&) = default;
        Signature& operator=(const Signature&) = delete;
        Signature(Signature&&) = delete;
        Signature& operator=(Signature&&) = delete;
        virtual ~Signature() = default;

        /** @brief Constructs Verify Class
         *
         * @param[in]
         */
        Signature(fs::path imageDirPath) : imageDirPath(imageDirPath) {};

        /**
         * @brief Verify the Manifest and public key file siganture using the
         *        public keys available in the system and then verify the image
         *        files signature using the public key and hash function in the
         *        image.
         *
         * @return true if signature verification was successful, false if not
         */
        bool verify();

    private:
        /**
         * @brief Wrapper function used for primary file validation
         *        verify the file using the available public keys
         *        and hash functions in in the system.
         *
         * @return true if signature verification was successful, false if not
         */
        bool primaryValidation(const std::string& name);

        /**
         * @brief Verify the file siganture using public key and hash function
         *
         * @param[in]  - Image file path
         * @param[in]  - Signature file path
         * @param[in]  - Public key
         * @param[in]  - Hash function name
         * @return true if signature verification was successful, false if not
         */
        bool verifyFile(fs::path file,
                        fs::path signature,
                        fs::path publicKey,
                        const std::string& hashFunc);

        /** @brief Directory where software images are placed*/
        fs::path imageDirPath;

        /** @brief Path where Public key from the image are placed*/
        fs::path publicKey;

        /** @brief hash function from the Manifest file*/
        std::string hashFunction;
};

} // namespace image
} // namespace software
} // namespace phosphor
