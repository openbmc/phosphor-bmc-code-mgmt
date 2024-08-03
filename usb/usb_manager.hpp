#pragma once

#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
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
    USBManager& operator=(USBManager&&) = delete;

#ifdef START_UPDATE_DBUS_INTEFACE

    explicit USBManager(sdbusplus::async::context& ctx, const fs::path& devPath,
                        const fs::path& usbPath) :
        ctx(ctx), devicePath(devPath), usbPath(usbPath)
    {
        ctx.spawn(run());
    }

    /** @brief Run the USBManager */
    auto run() -> sdbusplus::async::task<void>;

  private:
    /** @brief D-Bus context. */
    sdbusplus::async::context& ctx;

    /** @brief Starts the firmware update.
     *  @param[in]  fd  - The file descriptor of the image to update.
     *  @return Success or Fail
     */
    auto startUpdate(int fd) -> sdbusplus::async::task<bool>;

#else
    explicit USBManager(sdbusplus::bus_t& bus, sdeventplus::Event& event,
                        const fs::path& devPath, const fs::path& usbPath) :
        bus(bus), event(event), isUSBCodeUpdate(false),
        fwUpdateMatcher(bus,
                        MatchRules::interfacesAdded() +
                            MatchRules::path("/xyz/openbmc_project/software"),
                        std::bind(std::mem_fn(&USBManager::updateActivation),
                                  this, std::placeholders::_1)),
        devicePath(devPath), usbPath(usbPath)
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

  private:
    /** @brief Persistent sdbusplus DBus bus connection. */
    sdbusplus::bus_t& bus;

    /** sd event handler. */
    sdeventplus::Event& event;

    /** Indicates whether USB codeupdate is going on. */
    bool isUSBCodeUpdate;

    /** sdbusplus signal match for new image. */
    sdbusplus::bus::match_t fwUpdateMatcher;

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

#endif /* START_UPDATE_DBUS_INTEFACE */

    /** @brief Find the first file with a .tar extension according to the USB
     *         file path and copy to IMG_UPLOAD_DIR
     *
     *  @return Success or Fail
     */
    bool copyImage();

    /** The USB device path. */
    const fs::path& devicePath;

    /** The USB mount path. */
    const fs::path& usbPath;

    /** The destination path for copied over image file */
    fs::path imageDstPath;
};

} // namespace usb
} // namespace phosphor
