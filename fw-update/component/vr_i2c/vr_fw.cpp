#include "vr_fw.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

#include <cstdlib>

VRFW::VRFW(sdbusplus::async::context& io, sdbusplus::bus_t& bus,
		const std::string& swid, const char* objPath, I2CDevice* parend) :
	Software(io, objPath, swid, true), parent(parend),
	updateIntf(bus, objPath, this)
{
	lg2::debug("created VRFW instance");
}

