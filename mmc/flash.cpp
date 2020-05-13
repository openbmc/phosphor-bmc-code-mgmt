#include "config.h"

#include "flash.hpp"

#include "activation.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

void Activation::flashWrite()
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    auto serviceFile = "obmc-flash-mmc@" + versionId + ".service";
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

    auto mmcServiceFile = "obmc-flash-mmc@" + versionId + ".service";

    if (newStateUnit == mmcServiceFile && newStateResult == "done")
    {
        roVolumeCreated = true;
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
