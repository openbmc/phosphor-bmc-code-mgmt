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

void USBManager::setRequestedActivation(const std::string& path)
{
    utils::PropertyValue value =
        "xyz.openbmc_project.Software.Activation.RequestedActivations.Active";
    constexpr auto interface = "xyz.openbmc_project.Software.Activation";
    constexpr auto propertyName = "RequestedActivation";
    try
    {
        dBusHandler.setProperty(path, interface, propertyName, value);
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
                dBusHandler.getProperty(path.str, "Activation", imageInterface);
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