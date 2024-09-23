#include <cstdlib>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/server.hpp>

#include "bios_sw.hpp"
#include <xyz/openbmc_project/State/Host/client.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>

BiosSW::BiosSW(
	const std::shared_ptr<sdbusplus::asio::connection>& conn,const std::string& swid, const char* objPath, SPIDeviceCodeUpdater* parent):
	Software(conn, objPath, swid, true),
	parent(parent),
	updateIntf(*conn, objPath, this)
{
}

