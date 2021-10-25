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

    bool copyTar(const char* path);
    bool run();

  private:
    const char* usbPath;
};

} // namespace usb
} // namespace phosphor
