#pragma once

#include <phosphor-logging/lg2.hpp>

namespace phosphor
{
namespace usb
{
namespace fs = std::filesystem;

class USBManager
{
  public:
    ~USBManager() = default;
    USBManager() = delete;
    USBManager(const USBManager&) = delete;
    USBManager(USBManager&&) = default;
    USBManager& operator=(const USBManager&) = delete;
    USBManager& operator=(USBManager&&) = default;

    explicit USBManager(const fs::path& path) : usbPath(path)
    {}

    /** @brief Find the first file with a .tar extension according to the USB
     *         file path.
     *
     *  @return Success or Fail
     */
    bool run();

  private:
    /** The USB path detected. */
    const fs::path& usbPath;
};

} // namespace usb
} // namespace phosphor
