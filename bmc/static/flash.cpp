#include "config.h"

#include "flash.hpp"

#include "activation.hpp"
#include "images.hpp"
#include "item_updater.hpp"

#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <system_error>
#include <vector>

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

bool Activation::flashWrite()
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
        return true;
    }
#endif
    // For static layout code update, just put images in /run/initramfs.
    // It expects user to trigger a reboot and an updater script will program
    // the image to flash during reboot.
    fs::path uploadDir(IMG_UPLOAD_DIR);
    fs::path toPath(PATH_INITRAMFS);

    std::vector<fs::path> staged;
    auto stage = [&](const std::string& image, bool optional) {
        const fs::path from = uploadDir / versionId / image;
        std::error_code ec;
        if (optional && !fs::exists(from, ec))
        {
            return true;
        }

        staged.push_back(toPath / image);
        fs::copy_file(from, toPath / image,
                      fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            error("Failed to stage {IMAGE} to {DIR}: {ERROR}", "IMAGE", image,
                  "DIR", toPath, "ERROR", ec.message());
            return false;
        }
        return true;
    };

    bool success = true;
    for (const auto& bmcImage : parent.imageUpdateList)
    {
        if (!stage(bmcImage, false))
        {
            success = false;
            break;
        }
    }
    if (success)
    {
        for (const auto& optionalImage : getOptionalImages())
        {
            if (!stage(optionalImage, true))
            {
                success = false;
                break;
            }
        }
    }

    if (!success)
    {
        // Do not leave a partial set of images for the updater script.
        for (const auto& path : staged)
        {
            std::error_code ec;
            fs::remove(path, ec);
        }
    }

    return success;
}

void Activation::onStateChanges([[maybe_unused]] sdbusplus::message_t& msg)
{
#ifdef BMC_STATIC_DUAL_IMAGE
    uint32_t newStateID;
    auto serviceFile = FLASH_ALT_SERVICE_TMPL + versionId + ".service";
    sdbusplus::object_path newStateObjPath;
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
        Activation::activation(sdbusplus::server::xyz::openbmc_project::
                                   software::Activation::Activations::Failed);
    }
#endif
}

} // namespace updater
} // namespace software
} // namespace phosphor
