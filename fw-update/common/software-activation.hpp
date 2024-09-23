#pragma once

#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Software/Activation/aserver.hpp>

class SoftwareActivation :
    public sdbusplus::aserver::xyz::openbmc_project::software::Activation<
        SoftwareActivation>
{
  public:
    SoftwareActivation(sdbusplus::async::context& ctx, const char* objPath);
};
