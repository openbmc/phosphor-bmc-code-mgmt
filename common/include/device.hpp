#pragma once

#include "common/pldm/package_parser.hpp"
#include "software.hpp"
#include "software_config.hpp"

#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Association/Definitions/aserver.hpp>
#include <xyz/openbmc_project/Software/Activation/aserver.hpp>
#include <xyz/openbmc_project/Software/Update/aserver.hpp>
#include <xyz/openbmc_project/Software/Version/aserver.hpp>

#include <string>

using ActivationInterface =
    sdbusplus::common::xyz::openbmc_project::software::Activation;

namespace phosphor::software::manager
{
class SoftwareManager;
};

namespace phosphor::software::device
{

class Device
{
  public:
    Device(sdbusplus::async::context& ctx,
           const phosphor::software::config::SoftwareConfig& deviceConfig,
           phosphor::software::manager::SoftwareManager* parent,
           std::set<RequestedApplyTimes> allowedApplyTimes);

    virtual ~Device() = default;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    // @brief                      Applies the image to the device
    // @param image                raw fw image without pldm header
    // @param image_size           size of 'image'
    // @returns                    true if update was applied successfully.
    //                             Should also return true if update was applied
    //                             successfully, but the host failed to power
    //                             on.
    virtual sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                                      size_t image_size) = 0;

    // @brief               Set the ActivationProgress properties on dbus
    // @param progress      progress value
    // @returns             true on successful property update
    bool setUpdateProgress(uint8_t progress) const;

    // @brief                      This coroutine is spawned to perform the
    // async update of the device.
    // @param image                The memory fd with the pldm package
    // @param applyTime            When the update should be applied
    // @param swid                 The software id to use
    // @returns                    true if update was successfull
    sdbusplus::async::task<bool> startUpdateAsync(
        sdbusplus::message::unix_fd image, RequestedApplyTimes applyTime,
        std::unique_ptr<Software> softwareUpdateExternal);

    // Value of 'Type' field for the configuration in EM exposes record
    std::string getEMConfigType() const;

  protected:
    // The apply times for updates which are supported by the device
    // Override this if your device deviates from the default set of apply
    // times.
    std::set<RequestedApplyTimes> allowedApplyTimes;

    // software instance, identified by its swid
    // The specific derived class also owns its dbus interfaces,
    // which are destroyed when the instance is deleted.
    std::unique_ptr<Software> softwareCurrent;

    // In case of apply time == OnReset, this contains the software version
    // which has been written to the device, or should be written to it,
    // but is not active yet.
    std::unique_ptr<Software> softwarePending;

    // Resets the device, in whichever way is appropriate for the device.
    // The reset must be capable to apply the firmware update which was done
    // by 'deviceSpecificUpdateFunction', in case that function did not already
    // apply it. This method is optional to implement for that reason.
    virtual sdbusplus::async::task<bool> resetDevice();

    // The common configuration that all devices share.
    // We get this from EM configuration.
    config::SoftwareConfig config;

    manager::SoftwareManager* parent;

    sdbusplus::async::context& ctx;

  private:
    bool updateInProgress = false;

    // @param pldm_pkg             raw pldm package
    // @param packageParser        PackageParser instance
    // @param applyTime            when the update should be applied
    // @param softwarePendingIn    the pending software instance
    // @returns                    the return value of the device specific
    // update function
    sdbusplus::async::task<bool> continueUpdateWithMappedPackage(
        void* pldm_pkg,
        const std::shared_ptr<pldm::fw_update::PackageParser>& packageParser,
        sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
            RequestedApplyTimes applyTime,
        const std::unique_ptr<Software>& softwarePendingIn);

    friend update::SoftwareUpdate;
    friend Software;
};

}; // namespace phosphor::software::device
