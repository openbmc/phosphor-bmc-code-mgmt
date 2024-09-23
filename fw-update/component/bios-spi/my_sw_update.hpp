#pragma once

#include "phosphor-logging/lg2.hpp"

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

class BiosSW;

class MySwUpdate :
    sdbusplus::server::object_t<
        sdbusplus::server::xyz::openbmc_project::software::Update>
{
  public:
    MySwUpdate(sdbusplus::bus_t& bus, const char* path, BiosSW* bu) :
        sdbusplus::server::object_t<
            sdbusplus::server::xyz::openbmc_project::software::Update>(
            bus, path),
        bu(bu)
    {}

    BiosSW* bu;

    sdbusplus::message::object_path
        startUpdate(sdbusplus::message::unix_fd image,
                    sdbusplus::common::xyz::openbmc_project::software::
                        ApplyTime::RequestedApplyTimes applyTime) override;
    /** Get value of AllowedApplyTimes */
    std::set<sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
                 RequestedApplyTimes>
        allowedApplyTimes() const override;

    /** Set value of AllowedApplyTimes with option to skip sending signal */
    std::set<sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
                 RequestedApplyTimes>
        allowedApplyTimes(std::set<sdbusplus::common::xyz::openbmc_project::
                                       software::ApplyTime::RequestedApplyTimes>
                              value,
                          bool skipSignal) override;

    /** Set value of AllowedApplyTimes */
    std::set<sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
                 RequestedApplyTimes>
        allowedApplyTimes(std::set<sdbusplus::common::xyz::openbmc_project::
                                       software::ApplyTime::RequestedApplyTimes>
                              value) override;
};
