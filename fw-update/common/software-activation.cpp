#include "software-activation.hpp"

#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Software/Activation/aserver.hpp>

SoftwareActivation::SoftwareActivation(sdbusplus::async::context& ctx,
                                       const char* objPath) :
    sdbusplus::aserver::xyz::openbmc_project::software::Activation<
        SoftwareActivation>(ctx, objPath)
{
    emit_added();
};
