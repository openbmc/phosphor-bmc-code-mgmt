#include "software-association-definitions.hpp"

#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Association/Definitions/aserver.hpp>

SoftwareAssociationDefinitions::SoftwareAssociationDefinitions(
    sdbusplus::async::context& ctx, const char* objPath) :
    sdbusplus::aserver::xyz::openbmc_project::association::Definitions<
        SoftwareAssociationDefinitions>(ctx, objPath)
{
    emit_added();
};
