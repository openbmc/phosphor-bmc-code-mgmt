#include "config.h"

#include "msl_verify.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <xyz/openbmc_project/Software/Version/error.hpp>

int main(int argc, char* argv[])
{
    using MinimumShipLevel = openpower::software::image::MinimumShipLevel;
    //MinimumShipLevel minimumShipLevel(PHOSPHOR_MSL);
    MinimumShipLevel minimumShipLevel(BMC_MSL);

    if (!minimumShipLevel.verify())
    {
        using namespace phosphor::logging;
        using IncompatibleErr = sdbusplus::xyz::openbmc_project::Software::
            Version::Error::Incompatible;
        using Incompatible =
            xyz::openbmc_project::Software::Version::Incompatible;

        report<IncompatibleErr>(prev_entry<Incompatible::MIN_VERSION>(),
                                prev_entry<Incompatible::ACTUAL_VERSION>(),
                                prev_entry<Incompatible::VERSION_PURPOSE>());
    }

    return 0;
}
