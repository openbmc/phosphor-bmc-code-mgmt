#pragma once

#include <boost/asio/io_context.hpp>

#include <filesystem>

namespace phosphor::software::utils
{

namespace fs = std::filesystem;

struct RemovablePath
{
    fs::path path;

    explicit RemovablePath(const fs::path& path) : path(path) {}
    ~RemovablePath()
    {
        if (!path.empty())
        {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    }

    RemovablePath(const RemovablePath& other) = delete;
    RemovablePath& operator=(const RemovablePath& other) = delete;
    RemovablePath(RemovablePath&&) = delete;
    RemovablePath& operator=(RemovablePath&&) = delete;
};

/** @brief Untar the image
 *  @param[in] imageFd - The file descriptor of the image to untar.
 *  @param[in] extractDirPath - The destination directory for the untarred
 * image.
 *  @param[out] bool - The result of the untar operation.
 */
bool unTar(int imageFd, const std::string& extractDirPath);

} // namespace phosphor::software::utils

boost::asio::io_context& getIOContext();
