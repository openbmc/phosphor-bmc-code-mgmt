#pragma once
#include <experimental/filesystem>

namespace phosphor
{
namespace software
{
namespace image
{

namespace fs = std::experimental::filesystem;

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
        Signature(Signature&&) = default;
        Signature& operator=(Signature&&) = default;
        ~Signature() = default;

        /** @brief Constructs Verify Class
         *
         * @param[in]  imageDirPath - file path
         */
        Signature(const fs::path& imageDirPath) : imageDirPath(imageDirPath) {};

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

        /** @brief Directory where software images are placed*/
        fs::path imageDirPath;

};

} // namespace image
} // namespace software
} // namespace phosphor
