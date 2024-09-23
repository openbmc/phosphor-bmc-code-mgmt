#include <phosphor-logging/lg2.hpp>

#include "code_updater.hpp"
#include "sdbusplus/async/context.hpp"

#include <xyz/openbmc_project/State/Host/client.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>

#include "software.hpp"

sdbusplus::message::object_path Software::getObjectPath()
{
	std::string objPathStr = CodeUpdater::getObjPath(swid);
	return sdbusplus::message::object_path(objPathStr.c_str());
}
