#include "config.h"

#include "flash.hpp"

#include "activation.hpp"
#include "images.hpp"
#include "item_updater.hpp"

#include <phosphor-logging/lg2.hpp>

#include <filesystem>

namespace
{
constexpr auto PATH_INITRAMFS = "/run/initramfs";
constexpr auto FLASH_ALT_SERVICE_TMPL = "obmc-flash-bmc-alt@";
} // namespace

namespace phosphor
{
namespace software
{
namespace updater
{

PHOSPHOR_LOG2_USING;

namespace fs = std::filesystem;
using namespace phosphor::software::image;

void Activation::flashWrite()
{
#ifdef BMC_STATIC_DUAL_IMAGE
    if (parent.runningImageSlot != 0)
    {
        // It's running on the secondary chip, update the primary one
        info("Flashing primary flash from secondary, id: {ID}", "ID",
             versionId);
        auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                          SYSTEMD_INTERFACE, "StartUnit");
        auto serviceFile = FLASH_ALT_SERVICE_TMPL + versionId + ".service";
        method.append(serviceFile, "replace");
        bus.call_noreply(method);
        return;
    }
#endif
    // For static layout code update, just put images in /run/initramfs.
    // It expects user to trigger a reboot and an updater script will program
    // the image to flash during reboot.
    fs::path uploadDir(IMG_UPLOAD_DIR);
    fs::path toPath(PATH_INITRAMFS);

    for (const auto& bmcImage : parent.imageUpdateList)
    {
        fs::copy_file(uploadDir / versionId / bmcImage, toPath / bmcImage,
                      fs::copy_options::overwrite_existing);
    }
}

void Activation::onStateChanges(
    [[maybe_unused]] sdbusplus::message::message& msg)
{
#ifdef BMC_STATIC_DUAL_IMAGE
    uint32_t newStateID;
    auto serviceFile = FLASH_ALT_SERVICE_TMPL + versionId + ".service";
    sdbusplus::message::object_path newStateObjPath;
    std::string newStateUnit{};
    std::string newStateResult{};
    msg.read(newStateID, newStateObjPath, newStateUnit, newStateResult);

    if (newStateUnit != serviceFile)
    {
        return;
    }
    if (newStateResult == "done")
    {
        activationProgress->progress(90);
        onFlashWriteSuccess();
    }
    else
    {
        namespace softwareServer =
            sdbusplus::xyz::openbmc_project::Software::server;
        Activation::activation(softwareServer::Activation::Activations::Failed);
    }
#endif
}

} // namespace updater
} // namespace software
} // namespace phosphor
