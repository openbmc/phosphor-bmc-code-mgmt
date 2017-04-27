#pragma once

namespace phosphor
{
namespace software
{
namespace manager
{

/**
 * @brief Verify and untar the tarball. Verify the manifest file.
 *        Create and populate the version and filepath interfaces.
 *
 * @param[in]  tarballFilePath - Tarball path.
 * @param[in]  userdata        - Pointer to Watch object
 * @param[out] result          - 0 if successful.
 */
int processImage(const std::string& tarballFilePath, void* userdata);

/**
 * @brief Untar the tarball.
 *
 * @param[in]  tarballFilePath - Tarball path.
 * @param[in]  extractDirPath  - Dir path to extract tarball ball to.
 * @param[out] result          - 0 if successful.
 */
int unTar(const std::string& tarballFilePath,
          const std::string& extractDirPath);

} // namespace manager
} // namespace software
} // namespace phosphor
