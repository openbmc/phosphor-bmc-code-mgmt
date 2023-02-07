#include "config.h"

#include "usb_manager.hpp"

#include <sys/mount.h>

#include <system_error>

namespace phosphor
{
namespace usb
{

bool USBManager::run()
{
    std::error_code ec;
    fs::path dir(usbPath);
    fs::create_directories(dir, ec);

    auto rc = mount(devicePath.c_str(), usbPath.c_str(), "vfat", 0, NULL);
    if (rc)
    {
        lg2::error("Error ({ERRNO}) occurred during the mount call", "ERRNO",
                   errno);
        return false;
    }

    for (const auto& p : std::filesystem::directory_iterator(dir))
    {
        if (p.path().extension() == ".tar")
        {
            fs::path dstPath{IMG_UPLOAD_DIR / p.path().filename()};
            if (fs::exists(dstPath, ec))
            {
                lg2::info(
                    "{DSTPATH} already exists in the /tmp/images directory, exit the upgrade",
                    "DSTPATH", p.path().filename());

                break;
            }

            try
            {
                return fs::copy_file(fs::absolute(p.path()), dstPath);
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

void USBManager::setApplyTime()
{
    utils::PropertyValue value =
        "xyz.openbmc_project.Software.ApplyTime.RequestedApplyTimes.OnReset";
    try
    {
        constexpr auto objectPath = "/xyz/openbmc_project/software/apply_time";
        constexpr auto interface = "xyz.openbmc_project.Software.ApplyTime";
        constexpr auto propertyName = "RequestedApplyTime";
        utils::setProperty(bus, objectPath, interface, propertyName, value);
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to set RequestedApplyTime property, ERROR:{ERROR}",
                   "ERROR", e.what());
    }
}

void USBManager::setRequestedActivation(const std::string& path)
{
    utils::PropertyValue value =
        "xyz.openbmc_project.Software.Activation.RequestedActivations.Active";
    try
    {
        constexpr auto interface = "xyz.openbmc_project.Software.Activation";
        constexpr auto propertyName = "RequestedActivation";
        utils::setProperty(bus, path, interface, propertyName, value);
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to set RequestedActivation property, ERROR:{ERROR}",
                   "ERROR", e.what());
    }

    return;
}

void USBManager::updateActivation(sdbusplus::message_t& msg)
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
            auto imageProp = utils::getProperty<std::string>(
                bus, path.str, imageInterface, "Activation");

            if (imageProp == readyPro && isUSBCodeUpdate)
            {
                setApplyTime();
                setRequestedActivation(path.str);
                event.exit(0);
            }
        }
        catch (const std::exception& e)
        {
            lg2::error("Failed in getting Activation status, ERROR:{ERROR}",
                       "ERROR", e.what());
        }
    }
}

} // namespace usb
} // namespace phosphor
