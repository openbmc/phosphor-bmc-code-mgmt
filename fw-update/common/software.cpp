#include "software.hpp"

#include "fw_manager.hpp"
#include "sdbusplus/async/context.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

sdbusplus::message::object_path Software::getObjectPath()
{
    std::string objPathStr = FWManager::getObjPath(swid);
    return sdbusplus::message::object_path(objPathStr.c_str());
}
