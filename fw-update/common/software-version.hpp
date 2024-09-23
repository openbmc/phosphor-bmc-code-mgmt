#pragma once

#include <xyz/openbmc_project/Software/Version/aserver.hpp>

class SoftwareVersion :
    public sdbusplus::aserver::xyz::openbmc_project::software::Version<
        SoftwareVersion>
{
  public:
    SoftwareVersion(sdbusplus::async::context& ctx, const char* objPath);
};
