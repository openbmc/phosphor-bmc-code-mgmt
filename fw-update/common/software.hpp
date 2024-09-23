#pragma once

#include "software-activation.hpp"
#include "software-association-definitions.hpp"
#include "software-version.hpp"

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <xyz/openbmc_project/Association/Definitions/aserver.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/aserver.hpp>
#include <xyz/openbmc_project/Software/ActivationBlocksTransition/aserver.hpp>
#include <xyz/openbmc_project/Software/ActivationProgress/aserver.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/aserver.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

#include <string>

const auto actActive = sdbusplus::common::xyz::openbmc_project::software::
    Activation::Activations::Active;

class SoftwareActivationProgress :
    public sdbusplus::aserver::xyz::openbmc_project::software::
        ActivationProgress<SoftwareActivationProgress>
{
  public:
    SoftwareActivationProgress(sdbusplus::async::context& ctx,
                               const char* objPath) :
        sdbusplus::aserver::xyz::openbmc_project::software::ActivationProgress<
            SoftwareActivationProgress>(ctx, objPath),
        manager{ctx, objPath}
    {
        emit_added();
    };
    sdbusplus::server::manager_t manager;
};

class SoftwareActivationBlocksTransition :
    public sdbusplus::aserver::xyz::openbmc_project::software::
        ActivationBlocksTransition<SoftwareActivationBlocksTransition>
{
  public:
    SoftwareActivationBlocksTransition(sdbusplus::async::context& ctx,
                                       const char* objPath) :
        sdbusplus::aserver::xyz::openbmc_project::software::
            ActivationBlocksTransition<SoftwareActivationBlocksTransition>(
                ctx, objPath),
        manager{ctx, objPath}
    {
        emit_added();
    };
    sdbusplus::server::manager_t manager;
};

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

    // these are only required during the activation of the new fw
    // and are deleted again afterwards.
    std::shared_ptr<SoftwareActivationProgress> optSoftwareActivationProgress =
        nullptr;
    std::shared_ptr<SoftwareActivationBlocksTransition>
        optSoftwareActivationBlocksTransition = nullptr;
};
