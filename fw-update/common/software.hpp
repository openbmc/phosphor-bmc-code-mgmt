#pragma once

#include "software-activation.hpp"
#include "software-association-definitions.hpp"
#include "software-version.hpp"

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <xyz/openbmc_project/Association/Definitions/aserver.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/aserver.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

#include <string>

const auto actActive = sdbusplus::common::xyz::openbmc_project::software::
    Activation::Activations::Active;

// This represents a software version running on the device
class Software
{
  public:
    Software(sdbusplus::async::context& ctx, const char* objPath,
             const std::string& softwareId, bool isDryRun);

    sdbusplus::message::object_path getObjectPath();

    std::string swid;
    bool dryRun;

    sdbusplus::server::manager_t manager;

    SoftwareVersion softwareVersion;
    SoftwareActivation softwareActivation;
    SoftwareAssociationDefinitions softwareAssociationDefinitions;
};
