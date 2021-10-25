#pragma once

namespace phosphor
{
namespace usb
{

class USBManager
{
  public:
    ~USBManager() = default;
    USBManager() = delete;
    USBManager(const USBManager&) = delete;
    USBManager(USBManager&&) = default;
    USBManager& operator=(const USBManager&) = delete;
    USBManager& operator=(USBManager&&) = default;

    explicit USBManager(const char* path) : usbPath(path)
    {}

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

  private:
    /** The USB path detected. */
    const char* usbPath;
};

} // namespace usb
} // namespace phosphor
