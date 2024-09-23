#include "software.hpp"

#include "fw_manager.hpp"
#include "sdbusplus/async/context.hpp"
#include "sdbusplus/server/object.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

Software::Software(sdbusplus::bus_t& bus, const char* objPath,
                   const std::string& softwareId, const bool isDryRun) :
    swid(softwareId), dryRun(isDryRun), versionIntf(bus, objPath),
    activationIntf(bus, objPath), assocIntf(bus, objPath)
{
    lg2::debug("creating dbus interfaces for swid {SWID} on path {OBJPATH}",
               "SWID", softwareId, "OBJPATH", objPath);

    lg2::info(
        "TODO: create functional association from Version to Inventory Item");

    std::string inventoryPath = "/xyz/openbmc_project/TODO";
    assocIntf.associations(
        {{objPath, inventoryPath, "how_do_i_create_association"}}, false);

    activationIntf.activation(actActive, false);

    versionIntf.emit_added();
    activationIntf.emit_added();
    assocIntf.emit_added();
};

sdbusplus::message::object_path Software::getObjectPath()
{
    std::string objPathStr = FWManager::getObjPath(swid);
    return sdbusplus::message::object_path(objPathStr.c_str());
}
