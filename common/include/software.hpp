#pragma once

#include "software_update.hpp"

#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Association/Definitions/aserver.hpp>
#include <xyz/openbmc_project/Software/Activation/aserver.hpp>
#include <xyz/openbmc_project/Software/ActivationBlocksTransition/aserver.hpp>
#include <xyz/openbmc_project/Software/ActivationProgress/aserver.hpp>
#include <xyz/openbmc_project/Software/Version/aserver.hpp>

#include <string>

namespace phosphor::software::device
{
class Device;
}

namespace phosphor::software
{

// Need to declare this class to initialize the protected members of our base
// class. This prevents "Conditional jump or move depends on uninitialised
// value(s)" when properties are updated for the first time.
class SoftwareActivationProgress :
    private sdbusplus::aserver::xyz::openbmc_project::software::
        ActivationProgress<Software>
{
  public:
    SoftwareActivationProgress(sdbusplus::async::context& ctx,
                               const char* objPath);

    void setProgress(int progressArg);
};

using SoftwareActivationBlocksTransition = sdbusplus::aserver::xyz::
    openbmc_project::software::ActivationBlocksTransition<Software>;

using SoftwareVersion =
    sdbusplus::aserver::xyz::openbmc_project::software::Version<Software>;
using SoftwareActivation =
    sdbusplus::aserver::xyz::openbmc_project::software::Activation<Software>;
using SoftwareAssociationDefinitions =
    sdbusplus::aserver::xyz::openbmc_project::association::Definitions<
        Software>;

// This represents a software version running on the device.
class Software : private SoftwareActivation
{
  public:
    Software(sdbusplus::async::context& ctx, device::Device& parent);

    // Set the activation status of this software
    // @param activation         The activation status
    void setActivation(SoftwareActivation::Activations activation);

    // Add / remove the 'ActivationBlocksTransition' dbus interface.
    // This dbus interface is only needed during the update process.
    // @param enabled       determines if the dbus interface should be there
    void setActivationBlocksTransition(bool enabled);

    // This should populate 'softwareUpdate'
    // @param allowedApplyTimes        When updates to this Version can be
    // applied
    void enableUpdate(const std::set<RequestedApplyTimes>& allowedApplyTimes);

    // This should populate 'softwareVersion'
    // @param version      the version string
    void setVersion(const std::string& versionStr);

    // This should populate 'softwareAssociationDefinitions'
    // @param isRunning             if the software version is currently running
    // on the device. Otherwise the software is assumed to be activating (not
    // yet running).
    sdbusplus::async::task<> createInventoryAssociations(bool isRunning);

    // object path of this software
    sdbusplus::message::object_path objectPath;

    // The device we are associated to, meaning this software is either running
    // on the device, or could potentially run on that device (update).
    device::Device& parentDevice;

    // The software id
    const std::string swid;

    // This is only required during the activation of the new fw
    // and is deleted again afterwards.
    // This member is public since the device specific update function
    // needs to update the progress.
    std::unique_ptr<SoftwareActivationProgress> softwareActivationProgress =
        nullptr;

  protected:
    // @returns        a random software id (swid) for that device
    static std::string getRandomSoftwareId(device::Device& parent);

  private:
    Software(sdbusplus::async::context& ctx, device::Device& parent,
             const std::string& swid);

    // Dbus interface to prevent power state transition during update.
    std::unique_ptr<SoftwareActivationBlocksTransition>
        activationBlocksTransition = nullptr;

    // The software update dbus interface is not always present.
    // It is constructed if the software version is able to be updated.
    // For the new software version, this interface is constructed after the
    // update has taken effect
    std::unique_ptr<update::SoftwareUpdate> updateIntf = nullptr;

    // We do not know the software version until we parse the PLDM package.
    // Since the Activation interface needs to be available
    // before then, this is nullptr until we get to know the version.
    std::unique_ptr<SoftwareVersion> version = nullptr;

    // This represents our association to the inventory item in case
    // this software is currently on the device.
    std::unique_ptr<SoftwareAssociationDefinitions> associationDefinitions =
        nullptr;

    sdbusplus::async::context& ctx;

    friend update::SoftwareUpdate;
};

}; // namespace phosphor::software
