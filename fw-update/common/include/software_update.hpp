#pragma once

#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Software/Update/aserver.hpp>

class Software;

using RequestedApplyTimes = sdbusplus::common::xyz::openbmc_project::software::
    ApplyTime::RequestedApplyTimes;

class SoftwareUpdate :
    public sdbusplus::aserver::xyz::openbmc_project::software::Update<
        SoftwareUpdate>
{
  public:
    SoftwareUpdate(sdbusplus::async::context& ctx, const char* path,
                   Software& software,
                   const std::set<RequestedApplyTimes>& allowedApplyTimes);

    Software& software;

    auto method_call(start_update_t su, auto image, auto applyTime)
        -> sdbusplus::async::task<start_update_t::return_type>;

    auto set_property(allowed_apply_times_t aat, auto value) -> bool;
    auto get_property(allowed_apply_times_t aat) const;

  private:
    const std::set<RequestedApplyTimes> allowedApplyTimes;
};
