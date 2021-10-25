#include "config.h"

#include "usb_manager.hpp"

#include <filesystem>

namespace phosphor
{
namespace usb
{

bool USBManager::copyTar(const char* path)
{
    if (!path)
    {
        return false;
    }

    std::string cmd("/usr/bin/cp ");
    cmd += "-r ";
    cmd += path;
    cmd += " ";
    cmd += IMG_UPLOAD_DIR;

    return std::system(cmd.c_str()) == 0 ? true : false;
}

bool USBManager::run()
{
    namespace fs = std::filesystem;

    fs::path dir(usbPath);
    if (!fs::exists(dir))
    {
        return false;
    }

    for (const auto& p : std::filesystem::directory_iterator(dir))
    {
        if (p.path().extension() == ".tar")
        {
            return copyTar(p.path().c_str());
        }
    }

    return false;
}

} // namespace usb
} // namespace phosphor