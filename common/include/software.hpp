#pragma once

#include "software_update.hpp"

#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Association/Definitions/aserver.hpp>
#include <xyz/openbmc_project/Software/Activation/aserver.hpp>
#include <xyz/openbmc_project/Software/ActivationBlocksTransition/aserver.hpp>
#include <xyz/openbmc_project/Software/ActivationProgress/aserver.hpp>
#include <xyz/openbmc_project/Software/Version/aserver.hpp>

#include <string>

using SoftwareActivationProgress =
    sdbusplus::aserver::xyz::openbmc_project::software::ActivationProgress<
        Software>;

using SoftwareActivationBlocksTransition = sdbusplus::aserver::xyz::
    openbmc_project::software::ActivationBlocksTransition<Software>;

using SoftwareVersion =
    sdbusplus::aserver::xyz::openbmc_project::software::Version<Software>;
using SoftwareActivation =
    sdbusplus::aserver::xyz::openbmc_project::software::Activation<Software>;
using SoftwareAssociationDefinitions =
    sdbusplus::aserver::xyz::openbmc_project::association::Definitions<
        Software>;

class Device;

// This represents a software version running on the device.
// It's a composite object inheriting multiple dbus interfaces which are always
// exposed on this object.
class Software : private SoftwareActivation
{
  public:
    // construct with random software id
    Software(sdbusplus::async::context& ctx, Device& parent);

    // construct with defined software id
    Software(sdbusplus::async::context& ctx, const std::string& softwareId,
             Device& parent);

    // @returns        object path of this software
    sdbusplus::message::object_path getObjectPath() const;

    // Our software id
    const std::string swid;

    // Set the activation status of this software
    // @param activation         The activation status
    void setActivation(sdbusplus::common::xyz::openbmc_project::software::
                           Activation::Activations activation);

    // Add / remove the 'ActivationBlocksTransition' dbus interface.
    // This dbus interface is only needed during the update process.
    // @param enabled       determines if the dbus interface should be there
    void setActivationBlocksTransition(bool enabled);

    // This is only required during the activation of the new fw
    // and is deleted again afterwards.
    // This member is public since the device specific update function
    // needs to update the progress.
    std::unique_ptr<SoftwareActivationProgress> optSoftwareActivationProgress =
        nullptr;

    // This should populate 'optSoftwareUpdate'
    // @param allowedApplyTimes        When updates to this Version can be
    // applied
    void enableUpdate(const std::set<RequestedApplyTimes>& allowedApplyTimes);

    // This should populate 'optSoftwareVersion'
    // @param version      the version string
    void setVersion(const std::string& versionStr);

    // This should populate 'optSoftwareAssociationDefinitions'
    // @param isRunning             if the software version is currently running
    // on the device
    // @param isActivating          if the software version is currently
    // activating (not yet running) on the device
    sdbusplus::async::task<> setAssociationDefinitionsRunningActivating(
        bool isRunning, bool isActivating);

    // @returns this->parent
    Device& getParentDevice();

    // @returns        a random software id (swid) for that device
    static std::string getRandomSoftwareId(Device& parent);

    // @returns        the object path for the a software with that swid
    static std::string getObjPathFromSwid(const std::string& swid);

  private:
    Software(sdbusplus::async::context& ctx, std::string objPath,
             const std::string& softwareId, Device& parent);

    static long int getRandomId();

    // The device we are associated to, meaning this software is either running
    // on the device, or could potentially run on that device (update).
    Device& parent;

    // Dbus interface to prevent power state transition during update.
    std::unique_ptr<SoftwareActivationBlocksTransition>
        optActivationBlocksTransition = nullptr;

    // The software update dbus interface is not always present.
    // It is constructed if the software version is able to be updated.
    // For the new software version, this interface is constructed after the
    // update has taken effect (design: after 'Reset Device')
    std::unique_ptr<SoftwareUpdate> optSoftwareUpdate = nullptr;

    // We do not know the software version until we parse the PLDM package.
    // Since the Activation interface needs to be available
    // before then, this is nullptr until we get to know the version.
    std::unique_ptr<SoftwareVersion> optSoftwareVersion = nullptr;

    // This represents our association to the inventory item in case
    // this software is currently on the device.
    std::unique_ptr<SoftwareAssociationDefinitions>
        optSoftwareAssociationDefinitions = nullptr;

    sdbusplus::async::context& ctx;
};