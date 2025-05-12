#pragma once

#include <sdbusplus/bus.hpp>

#include <string>

namespace phosphor
{
namespace software
{
namespace updater
{

class Helper
{
  public:
    Helper(const Helper&) = delete;
    Helper& operator=(const Helper&) = delete;
    Helper(Helper&&) = default;
    Helper& operator=(Helper&&) = delete;
    ~Helper() = default;

    /** @brief Constructor
     *
     */
    explicit Helper(sdbusplus::bus_t& bus);

    /** @brief Do factory reset */
    static void factoryReset();

    /** @brief Remove the image with the flash id
     *
     * @param[in] flashId - The flash id of the image
     */
    void removeVersion(const std::string& flashId);

    /** @brief Update flash id in uboot env
     *
     * @param[in] flashId - The flash id of the image
     */
    void updateUbootVersionId(const std::string& flashId);

    /** @brief Mirror Uboot to the alt uboot partition */
    void mirrorAlt();

  private:
    /** @brief Persistent sdbusplus D-Bus bus connection. */
    sdbusplus::bus_t& bus;
};

} // namespace updater
} // namespace software
} // namespace phosphor
