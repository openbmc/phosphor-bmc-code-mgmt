#include "software.hpp"

#include "fw_manager.hpp"
#include "sdbusplus/async/context.hpp"
// #include "sdbusplus/server/object.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

Software::Software(sdbusplus::async::context& ctx, const char* objPath,
                   const std::string& softwareId, const bool isDryRun) :
    swid(softwareId), dryRun(isDryRun), manager{ctx, objPath},
    softwareVersion(ctx, objPath), softwareActivation(ctx, objPath),
    softwareAssociationDefinitions(ctx, objPath)
{
    lg2::debug("creating dbus interfaces for swid {SWID} on path {OBJPATH}",
               "SWID", softwareId, "OBJPATH", objPath);

    lg2::info(
        "TODO: create functional association from Version to Inventory Item");
};

sdbusplus::message::object_path Software::getObjectPath()
{
    std::string objPathStr = FWManager::getObjPath(swid);
    return sdbusplus::message::object_path(objPathStr.c_str());
}
