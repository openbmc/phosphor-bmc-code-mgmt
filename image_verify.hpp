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

        /** @brief Directory where software images are placed*/
        fs::path imageDirPath;

};

} // namespace image
} // namespace software
} // namespace phosphor
