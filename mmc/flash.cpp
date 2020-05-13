#include "config.h"

#include "flash.hpp"

#include "activation.hpp"
#include "images.hpp"

#include <filesystem>

namespace phosphor
{
namespace software
{
namespace updater
{

namespace fs = std::filesystem;
namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;
using namespace phosphor::software::image;

void Activation::flashWrite()
{
    // Write the kernel image file
    fs::copy_file(fs::path(IMG_UPLOAD_DIR) / versionId / kernelImage,
                  fs::path(BMC_KERNEL_PREFIX + versionId),
                  fs::copy_options::overwrite_existing);

    // Write the rootfs image file
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    auto serviceFile = "obmc-flash-bmc-mmc@" + versionId + ".service";
    method.append(serviceFile, "replace");
    bus.call_noreply(method);
}

void Activation::onStateChanges(sdbusplus::message::message& msg)
{
    uint32_t newStateID{};
    sdbusplus::message::object_path newStateObjPath;
    std::string newStateUnit{};
    std::string newStateResult{};

    // Read the msg and populate each variable
    msg.read(newStateID, newStateObjPath, newStateUnit, newStateResult);

    auto mmcServiceFile = "obmc-flash-bmc-mmc@" + versionId + ".service";

    if (newStateUnit == mmcServiceFile && newStateResult == "done")
    {
        roVolumeCreated = true;
        activationProgress->progress(activationProgress->progress() + 50);
    }

    if (newStateUnit == mmcServiceFile)
    {
        if (newStateResult == "failed" || newStateResult == "dependency")
        {
            Activation::activation(
                softwareServer::Activation::Activations::Failed);
        }
        else if (roVolumeCreated)
        {
            Activation::onFlashWriteSuccess();
        }
    }

    return;
}

} // namespace updater
} // namespace software
} // namespace phosphor
