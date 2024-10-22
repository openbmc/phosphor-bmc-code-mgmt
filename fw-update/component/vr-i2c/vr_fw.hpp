#pragma once

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

class MySwUpdate;

#include "fw-update/common/software.hpp"
#include "my_sw_update.hpp"
#include "i2c_device.hpp"

class VRFW : public Software
{
	public:
		VRFW(sdbusplus::asyn::context& io, sdbusplus::bus_t& bus,
			const std::string& swid, const char* objPath, I2CDevice* parend):

		I2CDevice* parent;
		MySwUpdate updateIntf;
}
