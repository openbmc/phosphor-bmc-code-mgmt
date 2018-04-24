#pragma once

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
    Sync() = default;
    Sync(const Sync&) = delete;
    Sync& operator=(const Sync&) = delete;
    Sync(Sync&&) = default;
    Sync& operator=(Sync&&) = default;
    ~Sync() = default;

    /**
     * @brief Process requested file or directory.
     * @param[out] result - 0 if successful.
     */
    int processEntry();
};

} // namespace manager
} // namespace software
} // namespace phosphor
