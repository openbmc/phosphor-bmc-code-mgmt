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

static void copyStaticFiles(const Activation& a)
{
    // For static layout code update, just put images in /run/initramfs.
    // It expects user to trigger a reboot and an updater script will program
    // the image to flash during reboot.
    fs::path uploadDir(IMG_UPLOAD_DIR);
    fs::path toPath(PATH_INITRAMFS);

    for (const auto& bmcImage : a.parent.imageUpdateList)
    {
        fs::copy_file(uploadDir / a.versionId / bmcImage, toPath / bmcImage,
                      fs::copy_options::overwrite_existing);
    }
}

static void restartUnit(sdbusplus::bus::bus& bus, const std::string& unit)
{
    try
    {
        auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                          SYSTEMD_INTERFACE, "RestartUnit");
        method.append(unit.c_str(), "replace");
        bus.call_noreply(method);
    }
    catch (const sdbusplus::exception::exception& ex)
    {
        error("Failed to restart service {UINT}, error: {ERR}", "UNIT", unit,
              "ERR", ex.what());
    }
}

void Activation::flashWrite()
{
#ifdef BMC_STATIC_DUAL_IMAGE
    // If in secondary:
    //   1. UpdateTarget == primary: Update primary by service;
    //   2. UpdateTarget == secondary: Update itself;
    //   3. UpdateTarget == both: Update primary by service, then update itself;
    // If in primary:
    //   1. UpdateTarget == primary: Update itself;
    //   2. UpdateTarget == secondary: Update secondary by service;
    //   3. UpdateTarget == both: Update secondary by service, then update
    //   itself;
    auto targetSlot = UpdateTarget::TargetSlot::Primary;
    if (updateTarget)
    {
        targetSlot = updateTarget->updateTargetSlot();
    }
    if ((parent.runningImageSlot != 0 &&
         targetSlot == UpdateTarget::TargetSlot::Primary) ||
        (parent.runningImageSlot == 0 &&
         targetSlot == UpdateTarget::TargetSlot::Secondary) ||
        targetSlot == UpdateTarget::TargetSlot::Both)
    {
        info("Flashing alt flash, id: {ID}", "ID", versionId);
        auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                          SYSTEMD_INTERFACE, "StartUnit");
        auto serviceFile = FLASH_ALT_SERVICE_TMPL + versionId + ".service";
        method.append(serviceFile, "replace");
        bus.call_noreply(method);
        updatingAltFlash = true;
        return;
    }

#endif
    copyStaticFiles(*this);
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
        auto progress = 90;
        if (updateTarget &&
            updateTarget->updateTargetSlot() == UpdateTarget::TargetSlot::Both)
        {
            // The alt update is done, update itself
            copyStaticFiles(*this);
            progress = 50;
        }
        activationProgress->progress(progress);
        onFlashWriteSuccess();
        updatingAltFlash = false;

        // Restart sync manager so that the files are synced
        constexpr auto syncManagerUnit =
            "xyz.openbmc_project.Software.Sync.service";
        restartUnit(bus, syncManagerUnit);
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
