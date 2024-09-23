#include "spi_device_sw_update.hpp"

#include "phosphor-logging/lg2.hpp"
#include "spi_device_sw.hpp"

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

sdbusplus::message::object_path
    MySwUpdate::startUpdate(sdbusplus::message::unix_fd image,
                            sdbusplus::common::xyz::openbmc_project::software::
                                ApplyTime::RequestedApplyTimes applyTime)
{
    lg2::info("requesting SPI Device update");
    // bu->startBIOSUpdate(image, applyTime); // old
    lg2::info("started asynchronous bios update with fd {FD}", "FD", image.fd);
    bu->parent->startUpdate(image, applyTime, bu->swid);

    return bu->getObjectPath();
}
/** Get value of AllowedApplyTimes */
std::set<sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
             RequestedApplyTimes>
    MySwUpdate::allowedApplyTimes() const
{
    using RequestedApplyTimes = sdbusplus::common::xyz::openbmc_project::
        software::ApplyTime::RequestedApplyTimes;
    return {RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};
}

/** Set value of AllowedApplyTimes with option to skip sending signal */
std::set<sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
             RequestedApplyTimes>
    MySwUpdate::allowedApplyTimes(
        std::set<sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
                     RequestedApplyTimes>
            value,
        bool skipSignal)
{
    (void)value;
    (void)skipSignal;
    return {};
}

/** Set value of AllowedApplyTimes */
std::set<sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
             RequestedApplyTimes>
    MySwUpdate::allowedApplyTimes(
        std::set<sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
                     RequestedApplyTimes>
            value)
{
    (void)value;
    return {};
}
