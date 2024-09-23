#include "software.hpp"

#include "fw_manager.hpp"
#include "sdbusplus/async/context.hpp"
#include "software-update.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Update/aserver.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

Software::Software(sdbusplus::async::context& ctx, const char* objPath,
                   const char* objPathConfig, const std::string& softwareId,
                   const bool isDryRun, Device& parent) :
    ctx(ctx), swid(softwareId), dryRun(isDryRun), softwareVersion(ctx, objPath),
    softwareActivation(ctx, objPath),
    softwareAssociationDefinitions(ctx, objPath), parent(parent)
{
    lg2::debug("creating dbus interfaces for swid {SWID} on path {OBJPATH}",
               "SWID", softwareId, "OBJPATH", objPath);

    lg2::info(
        "(TODO) create functional association from Version to Inventory Item");

    // we can associate to our configuration item, as discussed,
    // since the inventory item may be unclear/unavailable
    std::string forward = "config"; // we forward associate to our configuration
    std::string reverse = "software";
    std::string endpoint =
        std::string(objPathConfig); // our configuration object path
    softwareAssociationDefinitions.associations({{forward, reverse, endpoint}});
};

sdbusplus::message::object_path Software::getObjectPath() const
{
    std::string objPathStr = FWManager::getObjPathFromSwid(swid);
    return sdbusplus::message::object_path(objPathStr.c_str());
}

void Software::enableUpdate()
{
    std::string objPath = getObjectPath();

    if (this->optSoftwareUpdate != nullptr)
    {
        lg2::error("[Software] update of {OBJPATH} has already been enabled",
                   "OBJPATH", objPath);
        return;
    }

    lg2::info(
        "[Software] enabling update of {OBJPATH} (adding the update interface)",
        "OBJPATH", objPath);

    optSoftwareUpdate =
        std::make_shared<SoftwareUpdate>(this->ctx, objPath.c_str(), *this);
}
