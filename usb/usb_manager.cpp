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

void USBManager::setRequestedActivation(const std::string& path)
{
    utils::PropertyValue value =
        "xyz.openbmc_project.Software.Activation.RequestedActivations.Active";
    constexpr auto interface = "xyz.openbmc_project.Software.Activation";
    constexpr auto propertyName = "RequestedActivation";
    try
    {
        utils::setProperty(bus, path, interface, propertyName, value);
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to set RequestedActivation property, ERROR:{ERROR}",
                   "ERROR", e.what());
    }

    return;
}

void USBManager::updateActivation(sdbusplus::message::message& msg)
{
    std::map<std::string, std::map<std::string, std::variant<std::string>>>
        interfaces;
    sdbusplus::message::object_path path;
    msg.read(path, interfaces);

    constexpr auto imageInterface = "xyz.openbmc_project.Software.Activation";
    constexpr auto readyPro =
        "xyz.openbmc_project.Software.Activation.Activations.Ready";
    for (auto& interface : interfaces)
    {
        if (interface.first != imageInterface)
        {
            continue;
        }

        try
        {
            auto propVal =
                utils::getProperty(bus, path.str, "Activation", imageInterface);
            const auto& imageProp = std::get<std::string>(propVal);
            if (imageProp == readyPro && isUSBCodeUpdate)
            {
                setRequestedActivation(path.str);
            }
        }
        catch (const std::exception& e)
        {
            lg2::error("Failed in getting Activation status, ERROR:{ERROR}",
                       "ERROR", e.what());
        }
    }

    event.exit(0);
}

} // namespace usb
} // namespace phosphor