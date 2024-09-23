#pragma once

#include "software-activation.hpp"
#include "software-association-definitions.hpp"
#include "software-update.hpp"
#include "software-version.hpp"

#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Software/Activation/aserver.hpp>
#include <xyz/openbmc_project/Software/ActivationBlocksTransition/aserver.hpp>
#include <xyz/openbmc_project/Software/ActivationProgress/aserver.hpp>

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
            SoftwareActivationProgress>(ctx, objPath) {
            // emit_added();
        };
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
                ctx, objPath) {
                // emit_added();
            };
};

class Device;

// This represents a software version running on the device
class Software
{
  public:
    Software(sdbusplus::async::context& ctx, const char* objPath,
             const char* objPathConfig, const std::string& softwareId,
             bool isDryRun, Device& parent);

    sdbusplus::async::context& ctx;

    sdbusplus::message::object_path getObjectPath() const;

    std::string swid;
    bool dryRun;

    SoftwareVersion softwareVersion;
    SoftwareActivation softwareActivation;
    SoftwareAssociationDefinitions softwareAssociationDefinitions;

    // these are only required during the activation of the new fw
    // and are deleted again afterwards.
    std::shared_ptr<SoftwareActivationProgress> optSoftwareActivationProgress =
        nullptr;
    std::shared_ptr<SoftwareActivationBlocksTransition>
        optSoftwareActivationBlocksTransition = nullptr;

    // The software update dbus interface is not always present.
    // It is constructed if the software version is able to be updated.
    // For the new software version, this interface is constructed after the
    // update has taken effect (design: after 'Reset Device')
    std::shared_ptr<SoftwareUpdate> optSoftwareUpdate = nullptr;

    // This should populate 'optSoftwareUpdate'
    void enableUpdate();

    Device& parent;
};
