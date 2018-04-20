#pragma once
#include <experimental/filesystem>

namespace phosphor
{
namespace software
{
namespace manager
{

namespace fs = std::experimental::filesystem;
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
};

} // namespace manager
} // namespace software
} // namespace phosphor
