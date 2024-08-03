#include "config.h"

#include "usb_manager.hpp"

#include <sys/mount.h>

#include <xyz/openbmc_project/ObjectMapper/client.hpp>
#include <xyz/openbmc_project/Software/ApplyTime/common.hpp>
#include <xyz/openbmc_project/Software/Update/client.hpp>

#include <system_error>

namespace phosphor
{
namespace usb
{

using Association = std::tuple<std::string, std::string, std::string>;
using Paths = std::vector<std::string>;

bool USBManager::copyImage()
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
                imageDstPath = dstPath;
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

#ifdef START_UPDATE_DBUS_INTEFACE

// NOLINTNEXTLINE(readability-static-accessed-through-instance)
auto findAssociatedFunctionalPath(sdbusplus::async::context& ctx)
    -> sdbusplus::async::task<Paths>
{
    constexpr auto associatedPath =
        "/xyz/openbmc_project/software/bmc/functional";
    constexpr auto subTree = "/xyz/openbmc_project/software";
    std::vector<std::string> interfaces = {
        "xyz.openbmc_project.Association.Definitions"};

    try
    {
        using ObjectMapper =
            sdbusplus::client::xyz::openbmc_project::ObjectMapper<>;
        auto mapper = ObjectMapper(ctx)
                          .service(ObjectMapper::default_service)
                          .path(ObjectMapper::instance_path);

        co_return co_await mapper.get_associated_sub_tree_paths(
            associatedPath, subTree, 0, interfaces);
    }
    catch (std::exception& e)
    {
        lg2::error(
            "Exception occurred for GetAssociatedSubTreePaths for {PATH}: {ERROR}",
            "PATH", associatedPath, "ERROR", e);
    }
    co_return {};
}

// NOLINTNEXTLINE(readability-static-accessed-through-instance)
auto USBManager::startUpdate(int fd) -> sdbusplus::async::task<bool>
{
    using Updater = sdbusplus::client::xyz::openbmc_project::software::Update<>;
    using ApplyTimeIntf =
        sdbusplus::common::xyz::openbmc_project::software::ApplyTime;

    constexpr auto serviceName = "xyz.openbmc_project.Software.Manager";

    auto paths = co_await findAssociatedFunctionalPath(ctx);
    if (paths.size() != 1)
    {
        lg2::error("Failed to find associated functional path");
        co_return false;
    }

    auto updater = Updater(ctx).service(serviceName).path(paths[0]);
    sdbusplus::message::object_path objectPath = co_await updater.start_update(
        fd, ApplyTimeIntf::RequestedApplyTimes::OnReset);
    if (objectPath.str.empty())
    {
        lg2::error("StartUpdate failed");
        co_return false;
    }
    lg2::info("StartUpdate succeeded, objectPath: {PATH}", "PATH", objectPath);

    co_return true;
}

// NOLINTNEXTLINE(readability-static-accessed-through-instance)
auto USBManager::run() -> sdbusplus::async::task<void>
{
    auto res = copyImage();
    if (!res)
    {
        lg2::error("Failed to copy image from USB");
        co_return;
    }

    int fd = open(imageDstPath.c_str(), O_RDONLY);
    if (fd < 0)
    {
        lg2::error("Failed to open {PATH}", "PATH", imageDstPath);
        co_return;
    }

    co_await startUpdate(fd);

    ctx.request_stop();

    co_return;
}

#else

bool USBManager::run()
{
    return copyImage();
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

#endif // START_UPDATE_DBUS_INTEFACE

} // namespace usb
} // namespace phosphor
