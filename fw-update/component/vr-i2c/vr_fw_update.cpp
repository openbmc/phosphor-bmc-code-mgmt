#include "vr_fw_update.hpp"

#include "phosphor-logging/lg2.hpp"
#include "vr_fw.hpp"

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

sdbusplus::message::object_path
	VRFWUpdate::startUpdate(sdbusplus::message::unix_fd image,
			sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
				RequestedApplyTimes applyTime)
{
	lg2::info("requesting I2C VR device update");

	lg2::info("started asynchronous voltage regulator update with fd {FD}",
			"FD", image.fd);
	update->parent->startUpdate(image, applyTime, update->swid);

	return update->getObjectPath();
}

std::set<sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
		RequestedApplyTimes>
	VRFWUpdate::allowedApplyTimes() const
{
	using RequestedApplyTimes = sdbusplus::common::xyz::openbmc_project::
		software::ApplyTime::RequestedApplyTimes;
	return {RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};
}

std::set<sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
	RequestedApplyTimes>
	VRFWUpdate::allowedApplyTimes(
		std::set<sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
			RequestedApplyTimes>
			value, 
			bool skipSignal)
{
	(void)value;
	(void)skipSignal;
	return {};
}

std::set<sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
	RequestedApplyTimes>
		VRFWUpdate::allowedApplyTimes(
			std::set<sdbusplus::common::xyz::openbmc_project::software::
				ApplyTime::RequestedApplyTimes>
				value)
{
	(void)value;
	return {};
}
