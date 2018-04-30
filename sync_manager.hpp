#pragma once
#include <sdbusplus/bus.hpp>

namespace phosphor
{
namespace software
{
namespace manager
{

/** @class Sync
 *  @brief Contains filesystem sync functions.
 *  @details The software manager class that contains functions to perform
 *           sync operations.
 */
class Sync
{
  public:
    Sync(const Sync&) = delete;
    Sync& operator=(const Sync&) = delete;
    Sync(Sync&&) = default;
    Sync& operator=(Sync&&) = default;
    ~Sync() = default;

    /**
     * @brief Constructs Sync Class
     */
    Sync() {};

    /**
     * @brief Process requested file or directory.
     * @param[in] mask - The inotify mask.
     * @param[in] entryPath - The file or directory to process.
     * @param[out] result - 0 if successful.
     */
    int processEntry(int mask, const std::string& entryPath);
};

} // namespace manager
} // namespace software
} // namespace phosphor
