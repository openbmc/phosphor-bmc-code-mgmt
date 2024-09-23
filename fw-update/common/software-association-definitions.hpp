#pragma once

#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Association/Definitions/aserver.hpp>

class SoftwareAssociationDefinitions :
    public sdbusplus::aserver::xyz::openbmc_project::association::Definitions<
        SoftwareAssociationDefinitions>
{
  public:
    SoftwareAssociationDefinitions(sdbusplus::async::context& ctx,
                                   const char* objPath);
};
