#include "config.h"

#include "usb_manager.hpp"

#include <filesystem>

namespace phosphor
{
namespace usb
{

bool USBManager::run()
{
    fs::path dir(usbPath);
    if (!fs::exists(dir))
    {
        return false;
    }

    for (const auto& p : std::filesystem::directory_iterator(dir))
    {
        if (p.path().extension() == ".tar")
        {
            try
            {
                return fs::copy_file(p.path(), IMG_UPLOAD_DIR);
            }
            catch (const std::exception& e)
            {
                lg2::error("Error when copying {SRC} to /tmp/images: {ERROR}",
                           "SRC", p.path(), "ERROR", e.what());
            }

            break;
        }
    }

    return false;
}

} // namespace usb
} // namespace phosphor