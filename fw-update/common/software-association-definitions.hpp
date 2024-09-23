#pragma once

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <xyz/openbmc_project/Association/Definitions/aserver.hpp>

class SoftwareAssociationDefinitions :
    public sdbusplus::aserver::xyz::openbmc_project::association::Definitions<
        SoftwareAssociationDefinitions>
{
  public:
    SoftwareAssociationDefinitions(sdbusplus::async::context& ctx,
                                   const char* objPath);

    sdbusplus::server::manager_t manager;
};
