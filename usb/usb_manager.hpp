#pragma once

#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdeventplus/event.hpp>

namespace phosphor
{
namespace usb
{

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

    explicit USBManager(sdeventplus::Event& event, const char* path) :
        event(event), usbPath(path), isUSBCodeUpdate(false),
        fwUpdateMatcher(phosphor::usb::utils::DBusHandler::getBus(),
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

    /** @brief Copy the tar package to /tmp/images.
     *
     *  @param[in] path - The tar file path
     *
     *  @return Success or Fail
     */
    bool copyTar(const char* path);

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
    void updateActivation(sdbusplus::message::message& msg);

    /** @brief Method to set the RequestedActivation D-Bus property.
     *
     *  @param[in] path  - Update the object path of the firmware
     */
    void setRequestedActivation(const std::string& path);

  private:
    /** sd event handler. */
    sdeventplus::Event& event;

    /** The USB path detected. */
    const char* usbPath;

    /** Indicates whether USB codeupdate is going on. */
    bool isUSBCodeUpdate;

    /** sdbusplus signal match for new image. */
    sdbusplus::bus::match_t fwUpdateMatcher;

    /** DBusHandler class handles the D-Bus operations */
    utils::DBusHandler dBusHandler;
};

} // namespace usb
} // namespace phosphor
