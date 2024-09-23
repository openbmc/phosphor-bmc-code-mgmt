#include "software-version.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

SoftwareVersion::SoftwareVersion(sdbusplus::async::context& ctx,
                                 const char* objPath) :
    sdbusplus::aserver::xyz::openbmc_project::software::Version<
        SoftwareVersion>(ctx, objPath),
    manager{ctx, objPath}
{
    emit_added();
};
