#include "software-update.hpp"

#include "device.hpp"
#include "software.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Software/Update/aserver.hpp>

SoftwareUpdate::SoftwareUpdate(sdbusplus::async::context& ctx, const char* path,
                               Software& software) :
    sdbusplus::aserver::xyz::openbmc_project::software::Update<SoftwareUpdate>(
        ctx, path),
    software(software)
{}

auto SoftwareUpdate::method_call(start_update_t su, auto image, auto applyTime)
    -> sdbusplus::async::task<start_update_t::return_type>
{
    (void)su;
    lg2::info("requesting Device update");
    lg2::info("started asynchronous update with fd {FD}", "FD", image.fd);
    this->software.getParentDevice().startUpdate(image, applyTime,
                                                 this->software.swid);

    co_return this->software.getObjectPath();
}

auto SoftwareUpdate::set_property(allowed_apply_times_t aat, auto value) -> bool
{
    (void)aat;
    std::swap(allowed_apply_times_, value);
    return allowed_apply_times_ == value;
}
auto SoftwareUpdate::get_property(allowed_apply_times_t aat)
{
    (void)aat;
    using RequestedApplyTimes = sdbusplus::common::xyz::openbmc_project::
        software::ApplyTime::RequestedApplyTimes;
    return std::set{RequestedApplyTimes::Immediate,
                    RequestedApplyTimes::OnReset};
}
