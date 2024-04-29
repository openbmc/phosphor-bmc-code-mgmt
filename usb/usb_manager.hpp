#pragma once

#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdeventplus/event.hpp>

#include <filesystem>

namespace phosphor
{
namespace usb
{
namespace fs = std::filesystem;
namespace MatchRules = sdbusplus::bus::match::rules;

class USBManager
{
  public:
    ~USBManager() = default;
    USBManager() = delete;
    USBManager(const USBManager&) = delete;
    USBManager(USBManager&&) = default;
    USBManager& operator=(const USBManager&) = delete;
    USBManager& operator=(USBManager&&) = default;

    explicit USBManager(sdbusplus::bus_t& bus, sdeventplus::Event& event,
                        const fs::path& devPath, const fs::path& usbPath) :
        bus(bus), event(event), devicePath(devPath), usbPath(usbPath),
        isUSBCodeUpdate(false),
        fwUpdateMatcher(bus,
                        MatchRules::interfacesAdded() +
                            MatchRules::path("/xyz/openbmc_project/software"),
                        std::bind(std::mem_fn(&USBManager::updateActivation),
                                  this, std::placeholders::_1))
    {
        if (!run())
        {
            lg2::error("Failed to FW Update via USB, usbPath:{USBPATH}",
                       "USBPATH", usbPath);
            event.exit(0);
        }

        isUSBCodeUpdate = true;
    }

    /** @brief Find the first file with a .tar extension according to the USB
     *         file path.
     *
     *  @return Success or Fail
     */
    bool run();

    /** @brief Creates an Activation D-Bus object.
     *
     * @param[in]  msg   - Data associated with subscribed signal
     */
    void updateActivation(sdbusplus::message_t& msg);

    /** @brief Set Apply Time to OnReset.
     *
     */
    void setApplyTime();

    /** @brief Method to set the RequestedActivation D-Bus property.
     *
     *  @param[in] path  - Update the object path of the firmware
     */
    void setRequestedActivation(const std::string& path);

  private:
    /** @brief Persistent sdbusplus DBus bus connection. */
    sdbusplus::bus_t& bus;

    /** sd event handler. */
    sdeventplus::Event& event;

    /** The USB device path. */
    const fs::path& devicePath;

    /** The USB mount path. */
    const fs::path& usbPath;

    /** Indicates whether USB codeupdate is going on. */
    bool isUSBCodeUpdate;

    /** sdbusplus signal match for new image. */
    sdbusplus::bus::match_t fwUpdateMatcher;
};

} // namespace usb
} // namespace phosphor
