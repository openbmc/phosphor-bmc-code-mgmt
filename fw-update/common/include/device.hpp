#pragma once

#include "software.hpp"

#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Association/Definitions/aserver.hpp>
#include <xyz/openbmc_project/Software/Activation/aserver.hpp>
#include <xyz/openbmc_project/Software/Update/aserver.hpp>
#include <xyz/openbmc_project/Software/Version/aserver.hpp>

#include <string>

class SoftwareManager;

// represents an arbitrary device which has firmware.
// Some devices can be updated.
class Device
{
  public:
    Device(sdbusplus::async::context& io, bool dryRun, uint32_t vendorIANA,
           const std::string& compatible, const std::string& objPathConfig,
           SoftwareManager* parent);
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
    virtual sdbusplus::async::task<bool> deviceSpecificUpdateFunction(
        const uint8_t* image, size_t image_size,
        std::unique_ptr<SoftwareActivationProgress>& activationProgress) = 0;

    // @param image                pldm fw update package file descriptor
    // @param applyTime            when the update should be applied
    // @param oldSwId              the swid of the software we are replacing
    virtual void startUpdate(sdbusplus::message::unix_fd image,
                             sdbusplus::common::xyz::openbmc_project::software::
                                 ApplyTime::RequestedApplyTimes applyTime,
                             const std::string& oldSwId) final;

    // Value of 'Type' field for the configuration in EM exposes record
    std::string getEMConfigType();

    // software instance, identified by their swid
    // The specific derived class also owns its dbus interfaces,
    // which are destroyed when the instance is deleted from this map
    std::unique_ptr<Software> softwareCurrent;

    virtual int getUpdateTimerDelaySeconds();

  protected:
    std::unique_ptr<Software> softwareUpdate;

    // Resets the device, in whichever way is appropriate for the device.
    // The reset must be capable to apply the firmware update which was done
    // by 'deviceSpecificUpdateFunction', in case that function did not already
    // apply it. This method is optional to implement for that reason.
    virtual void resetDevice();

    // the dbus object path of our configuration
    std::string objPathConfig;

    SoftwareManager* parent;
    bool dryRun;
    sdbusplus::bus_t& bus;
    sdbusplus::async::context& io;

    // https://gerrit.openbmc.org/c/openbmc/docs/+/74653
    // properties from the configuration
    uint32_t vendorIANA; // "0x0000A015", 4 bytes as per PLDM spec
    std::string
        compatible;      // e.g.
                    // "com.meta.Hardware.Yosemite4.MedusaBoard.CPLD.LCMX02_2000HC"

    // members to handle the asynchronous update
    sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
        RequestedApplyTimes applyTime;
    sdbusplus::message::unix_fd image;
    std::string oldSwId; // to be deleted on update completion

  private:
    // @param image                pldm fw update package file descriptor
    // @param applyTime            when the update should be applied
    // @returns                    the return value of
    // 'continueUpdateWithMappedPackage'
    sdbusplus::async::task<bool>
        continueUpdate(sdbusplus::message::unix_fd image,
                       sdbusplus::common::xyz::openbmc_project::software::
                           ApplyTime::RequestedApplyTimes applyTime);

    // @param pldm_pkg             raw pldm package
    // @param pldm_pkg_size        size of 'pldm_pkg'
    // @param applyTime            when the update should be applied
    // @returns                    the return value of the device specific
    // update function
    sdbusplus::async::task<bool> continueUpdateWithMappedPackage(
        void* pldm_pkg, size_t pldm_pkg_size,
        sdbusplus::common::xyz::openbmc_project::software::ApplyTime::
            RequestedApplyTimes applyTime);

    sdbusplus::async::task<bool> startUpdateAsync();
};
