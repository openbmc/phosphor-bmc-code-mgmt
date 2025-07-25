#pragma once

#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Software/Update/aserver.hpp>

namespace phosphor::software
{
class Software;
};

using RequestedApplyTimes = sdbusplus::common::xyz::openbmc_project::software::
    ApplyTime::RequestedApplyTimes;

namespace phosphor::software::update
{

class SoftwareUpdate :
    public sdbusplus::aserver::xyz::openbmc_project::software::Update<
        SoftwareUpdate>
{
  public:
    SoftwareUpdate(const SoftwareUpdate&) = delete;
    SoftwareUpdate(SoftwareUpdate&&) = delete;
    SoftwareUpdate& operator=(const SoftwareUpdate&) = delete;
    SoftwareUpdate& operator=(SoftwareUpdate&&) = delete;
    SoftwareUpdate(sdbusplus::async::context& ctx, const char* path,
                   Software& software,
                   const std::set<RequestedApplyTimes>& allowedApplyTimes);

    ~SoftwareUpdate();

    auto method_call(start_update_t su, auto image, auto applyTime)
        -> sdbusplus::async::task<start_update_t::return_type>;

    auto get_property(allowed_apply_times_t aat) const;

  private:
    Software& software;

    const std::set<RequestedApplyTimes> allowedApplyTimes;
};

}; // namespace phosphor::software::update
