#pragma once
#include <experimental/filesystem>
#include "key_manager.hpp"

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

        /** @brief Constructs Signature.
         * @param[in]  imageDirPath - image path
         * @details Initlizes the KeyManager object based on the image
         *          directory path. KeyManager object provides image
         *          specific configurations information required for
         *          signature vaidation.
         */
        Signature(fs::path imageDirPath):
            imageDirPath(imageDirPath),
            keyManager(imageDirPath)
        {};

        /**
         * @brief Image signature verification function.
         *        Verify the Manifest and public key file signature using the
         *        public keys available in the system first. After successful
         *        validation, continue the whole image files signature
         *        validation using the image specific public key and the
         *        hash function.
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
        bool verifyFile(const fs::path& file,
                        const fs::path& signature,
                        const fs::path& publicKey,
                        const std::string& hashFunc);

        /** @brief Directory where software images are placed*/
        fs::path imageDirPath;

        /** Key manager object */
        KeyManager keyManager;
};

} // namespace image
} // namespace software
} // namespace phosphor
