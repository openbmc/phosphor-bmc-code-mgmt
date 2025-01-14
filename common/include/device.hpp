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

class SoftwareManager;

class Device
{
  public:
    Device(sdbusplus::async::context& ctx, bool dryRun,
           const SoftwareConfig& deviceConfig, SoftwareManager* parent);

    virtual ~Device() = default;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    // we provide the pointer to the dbus interface so the device specific
    // implementation can give percentage progress updates
    // @param image                raw fw image without pldm header
    // @param image_size           size of 'image'
    // @param activationProgress   dbus interface for progress reporting
    // @returns                    true if update was applied successfully.
    //                             Should also return true if update was applied
    //                             successfully, but the host failed to power
    //                             on.
    virtual sdbusplus::async::task<bool> updateDevice(
        const uint8_t* image, size_t image_size,
        std::unique_ptr<SoftwareActivationProgress>& activationProgress) = 0;

    // @returns                    the object path of the inventory item which
    //                             can be associated with this device.
    virtual sdbusplus::async::task<std::string>
        getInventoryItemObjectPath() = 0;

    // Returns the apply times for updates which are supported by the device
    // Override this if your device deviates from the default set of apply
    // times.
    virtual std::set<RequestedApplyTimes> allowedApplyTimes();

    // @param image                the memory fd with the pldm package
    // @param applyTime            when the update should be applied
    // @param swid                 the software id to use
    // @returns                    true if update was successfull
    sdbusplus::async::task<bool> startUpdateAsync(
        sdbusplus::message::unix_fd image, RequestedApplyTimes applyTime,
        std::unique_ptr<Software> softwareUpdate);

    // Value of 'Type' field for the configuration in EM exposes record
    std::string getEMConfigType() const;

    // software instance, identified by its swid
    // The specific derived class also owns its dbus interfaces,
    // which are destroyed when the instance is deleted.
    std::unique_ptr<Software> softwareCurrent;

  protected:
    // Resets the device, in whichever way is appropriate for the device.
    // The reset must be capable to apply the firmware update which was done
    // by 'deviceSpecificUpdateFunction', in case that function did not already
    // apply it. This method is optional to implement for that reason.
    virtual void resetDevice();

    // The common configuration that all devices share.
    // We get this from EM configuration.
    SoftwareConfig config;

    SoftwareManager* parent;
    bool dryRun;

    sdbusplus::async::context& ctx;

  private:
    // @param pldm_pkg             raw pldm package
    // @param pldm_pkg_size        size of 'pldm_pkg'
    // @param applyTime            when the update should be applied
    // @returns                    the return value of the device specific
    // update function
    sdbusplus::async::task<bool> continueUpdateWithMappedPackage(
        void* pldm_pkg,
        const std::shared_ptr<pldm::fw_update::PackageParser>& packageParser,
        sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
            RequestedApplyTimes applyTime,
        const std::unique_ptr<Software>& softwareUpdate);

    friend SoftwareUpdate;
    friend Software;
};
