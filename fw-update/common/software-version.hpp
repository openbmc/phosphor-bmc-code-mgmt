#pragma once

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/aserver.hpp>

#include <string>

class SoftwareVersion :
    public sdbusplus::aserver::xyz::openbmc_project::software::Version<
        SoftwareVersion>
{
  public:
    SoftwareVersion(sdbusplus::async::context& ctx, const char* objPath);

    sdbusplus::server::manager_t manager;
};
