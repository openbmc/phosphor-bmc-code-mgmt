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

  private:
    bool copyTar(const char* path);
    bool run();
    /** @brief Callback function for USB code update match.
     *  @details Creates an Activation D-Bus object.
     *
     * @param[in]  msg       - Data associated with subscribed signal
     */
    void updateActivation(sdbusplus::message::message& msg);
    void setRequestedActivation(const std::string& path);

  private:
    /**
     * @brief sd event handler.
     */
    sdeventplus::Event& event;

    /**
     * @brief The USB path.
     */
    const char* usbPath;

    /**
     * @brief Indicates whether USB codeupdate is going on.
     */
    bool isUSBCodeUpdate;

    /**
     * @brief sdbusplus signal match for new image.
     */
    sdbusplus::bus::match_t fwUpdateMatcher;

    /** DBusHandler class handles the D-Bus operations */
    utils::DBusHandler dBusHandler;
};

} // namespace usb
} // namespace phosphor
