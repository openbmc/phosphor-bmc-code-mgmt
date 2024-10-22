#pragma once

#include "phosphor-logging/lg2.hpp"

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

class VRFW;

class VRFWUpdate : sdbusplus::server::object_t<
	sdbusplus::server::xyz::openbmc_project::software::Update>
{
	public:
		VRFWUpdate(sdbusplus::bus_t& bus, const char* path, VRFW* update) :
			sdbusplus::server::object_t<
				sdbusplus::server::xyz::openbmc_project::software::Update>(
              		bus, path),
				update(update)
	{}

	const std::shared_ptr<VRFW> update;

	sdbusplus::message::object_path
		startUpdate(sdbusplus::message::unix_fd image,
				sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
				RequestedApplyTimes applyTime) override;

	std::set<sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
		RequestedApplyTimes>
			allowedApplyTimes() const override;

	std::set<sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
        RequestedApplyTimes>
        	allowedApplyTimes(std::set<sdbusplus::common::xyz::openbmc_project::
                software::ApplyTime::RequestedApplyTimes> value,
                          bool skipSignal) override;

	
	std::set<sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
    	RequestedApplyTimes> allowedApplyTimes(std::set<sdbusplus::
			common::xyz::openbmc_project::software::
				ApplyTime::RequestedApplyTimes> value) override;
};

