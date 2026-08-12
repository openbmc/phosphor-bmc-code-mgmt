#pragma once

#include "common/include/software_manager.hpp"
#include "ocp_recovery_device.hpp"

#include <sdbusplus/async/context.hpp>

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
    // All OCPRecoveryFirmware configs fold into one aggregated device
    // advertising a single Software.Update object. Owned by the base
    // class 'devices' map; anchored to the first-seen config path.
    phosphor::software::ocp_recovery::device::OCPRecoveryDevice*
        aggregateDevice = nullptr;
};
