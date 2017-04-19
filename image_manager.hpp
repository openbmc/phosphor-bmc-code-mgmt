#pragma once

namespace phosphor
{
namespace software
{
namespace manager
{

/** @class Image
 *  @brief A software image.
 *  @details The software image class verifies and untars the tarball, verifies
 *  the manifest file, and uses the manifest file to create and populate
 *  the version and filepath interfaces.
 */
class Image
{
    public:
        /**
         * @brief Verify and untar the tarball. Verify the manifest file.
         *        Create and populate the version and filepath interfaces.
         *
         * @param[in]  tarballFilePath - Tarball path.
         * @param[out] result          - 0 if successful.
         */
        static int processImage(const std::string& tarballFilePath);
};

} // namespace manager
} // namespace software
} // namespace phosphor
