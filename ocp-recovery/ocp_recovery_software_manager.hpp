#pragma once

#include "common/include/software_manager.hpp"
#include "ocp_recovery_device.hpp"

#include <sdbusplus/async/context.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <tuple>

namespace ManagerInf = phosphor::software::manager;
namespace SDBusAsync = sdbusplus::async;

class OCPRecoverySoftwareManager : public ManagerInf::SoftwareManager
{
  public:
    OCPRecoverySoftwareManager(SDBusAsync::context& ctx);

    SDBusAsync::task<bool> initDevice(const std::string& service,
                                      const sdbusplus::object_path& path,
                                      SoftwareConfig& config) final;

    void start();

  private:
    // Address, ChunkSize, MemoryWindow, timeout, VendorIANA, CompatibleHardware
    using DeviceSettings =
        std::tuple<uint16_t, size_t, uint8_t, uint32_t, uint32_t, std::string>;

    // Configs with identical settings fold into one aggregated device and
    // Software.Update object, owned by the base class 'devices' map
    std::map<DeviceSettings,
             phosphor::software::ocp_recovery::device::OCPRecoveryDevice*>
        aggregateDevices;
};
