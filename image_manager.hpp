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
 * @param[out] result          - 0 if successful.
 */
int processImage(const std::string& tarballFilePath);

} // namespace manager
} // namespace software
} // namespace phosphor
