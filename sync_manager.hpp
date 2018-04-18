#pragma once
#include <experimental/filesystem>
#include <sdbusplus/bus.hpp>

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
    Sync() = delete;
    Sync(const Sync&) = delete;
    Sync& operator=(const Sync&) = delete;
    Sync(Sync&&) = default;
    Sync& operator=(Sync&&) = default;
    ~Sync() = default;

    /**
     * @brief Constructs Sync Class
     * @param[in] bus - The Dbus bus object
     */
    Sync(sdbusplus::bus::bus& bus){};

    /**
     * @brief Sync requested file.
     * @param[out] result - 0 if successful.
     */
    int processFile();
};

} // namespace manager
} // namespace software
} // namespace phosphor
