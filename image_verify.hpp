#pragma once
#include <openssl/rsa.h>
#include <experimental/filesystem>
#include <unistd.h>
#include <sys/mman.h>

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
/** @struct CustomFd
 *
 *  RAII wrapper for file descriptor.
 */
struct CustomFd
{
    public:
        CustomFd() = delete;
        CustomFd(const CustomFd&) = delete;
        CustomFd& operator=(const CustomFd&) = delete;
        CustomFd(CustomFd&&) = delete;
        CustomFd& operator=(CustomFd&&) = delete;
        /** @brief Saves File descriptor and uses it to do file operation
          *
          *  @param[in] fd - File descriptor
          */
        CustomFd(int fd) : fd(fd) {}

        ~CustomFd()
        {
            if (fd >= 0)
            {
                close(fd);
            }
        }

        int operator()() const
        {
            return fd;
        }

    private:
        /** @brief File descriptor */
        int fd = -1;
};

/** @struct CustomMap
 *
 *  RAII wrapper for mmap.
 */
struct CustomMap
{
    private:
        /** @brief starting address of the map   */
        void* addr;

        /** @brief length of the mapping   */
        size_t length;

    public:
        CustomMap() = delete;

        /** @brief Saves starting address of the map and
         *         and length of the file.
         *  @param[in]  addr - Starting address of the map
         *  @param[in]  length - length of the map
         */
        CustomMap(void* addr,  size_t length) :
            addr(addr),
            length(length) {}

        ~CustomMap()
        {
            munmap(addr, length);
        }

        void* operator()() const
        {
            return addr;
        }
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

        /** passing paths for unit testing purpose */
        Signature(const fs::path& imageDirPath,
                  const fs::path& signedConfPath):
            imageDirPath(imageDirPath),
            keyManager(imageDirPath, signedConfPath)
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

        /**
         * @brief Create RSA object from the public key
         * @param[in]  - publickey
         * @param[out] - RSA Object.
         */
        inline RSA* createPublicRSA(const fs::path& publicKey);

        /**
         * @brief Memory map the  file
         * @param[in]  - file path
         * @param[in]  - file size
         * @param[out] - Custom Mmap address
         */
        CustomMap mapFile(const fs::path& path, size_t size);

        /** @brief Directory where software images are placed*/
        fs::path imageDirPath;

        /** Key manager object */
        KeyManager keyManager;
};

} // namespace image
} // namespace software
} // namespace phosphor
