#pragma once

#include <string>
#include <sdbusplus/bus.hpp>

namespace phosphor
{
namespace software
{
namespace updater
{

class Helper
{
  public:
    Helper() = delete;
    Helper(const Helper&) = delete;
    Helper& operator=(const Helper&) = delete;
    Helper(Helper&&) = default;
    Helper& operator=(Helper&&) = default;

    /** @brief Constructor
     *
     *  @param[in] bus - sdbusplus D-Bus bus connection
     */
    Helper(sdbusplus::bus::bus& bus) : bus(bus)
    {
        // Empty
    }

    void clearEntry(const std::string& entryId);
    void cleanup();
    void factoryReset();
    void removeVersion(const std::string& versionId);
    void updateVersion(const std::string& versionId);
    void enableFieldMode();
    void mirrorAlt();

  private:
    /** @brief Persistent sdbusplus D-Bus bus connection. */
    sdbusplus::bus::bus& bus;
};

} // namespace updater
} // namespace software
} // namespace phosphor
