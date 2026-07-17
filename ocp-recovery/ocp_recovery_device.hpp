#pragma once

#include "common/include/device.hpp"
#include "common/include/software_config.hpp"
#include "common/include/software_manager.hpp"

#include <ocp/ocp_recovery.h>

#include <sdbusplus/async/context.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace SoftwareInf = phosphor::software;
namespace ManagerInf = SoftwareInf::manager;
namespace DeviceInf = SoftwareInf::device;
namespace ConfigInf = SoftwareInf::config;

namespace SDBusPlusSoftware = sdbusplus::common::xyz::openbmc_project::software;

namespace phosphor::software::ocp_recovery::device
{

class OCPRecoveryDevice : public DeviceInf::Device
{
  public:
    using DeviceInf::Device::softwareCurrent;
    OCPRecoveryDevice(sdbusplus::async::context& ctx,
                      std::vector<uint16_t> buses, uint16_t address,
                      size_t chunkSize, uint8_t cms,
                      uint32_t forceRecoveryTimeout,
                      ConfigInf::SoftwareConfig& config,
                      ManagerInf::SoftwareManager* parent) :
        DeviceInf::Device(
            ctx, config, parent,
            {SDBusPlusSoftware::ApplyTime::RequestedApplyTimes::Immediate}),
        buses(std::move(buses)), address(address), chunkSize(chunkSize),
        cms(cms), forceRecoveryTimeout(forceRecoveryTimeout)
    {}

    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) final;

    // OCP recovery consumes every applicable component image of the
    // package: each one is staged into the CMS and activated in order
    // within a single recovery session. The session is attempted on
    // every configured bus (fleet semantics): healthy devices are
    // skipped, devices in recovery mode (or forced there) are
    // recovered, unreachable buses are skipped with a log.
    sdbusplus::async::task<bool> updateDeviceComponents(
        const std::vector<DeviceInf::ComponentImage>& components) final;

    // Additional entity-manager configs of the same recovery class fold
    // their bus into this (single, aggregated) device.
    void addBus(uint16_t bus)
    {
        if (std::find(buses.begin(), buses.end(), bus) == buses.end())
        {
            buses.push_back(bus);
        }
    }

  private:
    enum class BusOutcome
    {
        recovered,
        skippedHealthy,
        unreachable,
        failed,
    };

    // Full recovery attempt for one bus. 'written' accumulates staged
    // bytes across all buses and components for overall progress
    // against 'grandTotal'.
    sdbusplus::async::task<BusOutcome> recoverBus(
        uint16_t bus, const std::vector<DeviceInf::ComponentImage>& components,
        size_t& written, size_t grandTotal);

    sdbusplus::async::task<bool> ensureRecoveryMode(struct ocp_ctx* ocp,
                                                    uint16_t bus);

    // Writes one image; 'written' accumulates and maps overall progress
    // against 'grandTotal'.
    sdbusplus::async::task<bool> writeImage(struct ocp_ctx* ocp, uint16_t bus,
                                            const uint8_t* image, size_t size,
                                            size_t& written, size_t grandTotal);

    sdbusplus::async::task<bool> pollRecoveryComplete(struct ocp_ctx* ocp,
                                                      uint16_t bus);

    std::vector<uint16_t> buses;
    uint16_t address;
    size_t chunkSize;
    uint8_t cms;
    uint32_t forceRecoveryTimeout; // seconds
};

} // namespace phosphor::software::ocp_recovery::device
