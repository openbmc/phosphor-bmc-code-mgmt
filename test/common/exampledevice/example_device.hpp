#pragma once

#include "common/include/device.hpp"
#include "common/include/software_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

class ExampleCodeUpdater : public phosphor::software::manager::SoftwareManager
{
  public:
    ExampleCodeUpdater(sdbusplus::async::context& ctx,
                       long uniqueSuffix = getRandomId());

    sdbusplus::async::task<> initDevice(const std::string& service,
                                        const std::string& path,
                                        SoftwareConfig& config) final;

  private:
    static long getRandomId();
};

const std::string exampleName = "ExampleSoftware";

const uint32_t exampleVendorIANA = 0x0000a015;
const std::string exampleCompatibleHardware = "com.example.CompatibleDevice";

const std::string exampleInvObjPath =
    "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleDevice";

class ExampleDevice : public Device
{
  public:
    using Device::softwarePending;
    using phosphor::software::device::Device::softwareCurrent;

    static SoftwareConfig defaultConfig;

    ExampleDevice(sdbusplus::async::context& ctx,
                  phosphor::software::manager::SoftwareManager* parent,
                  const SoftwareConfig& config = defaultConfig);

    // NOLINTBEGIN(readability-static-accessed-through-instance)
    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) override;
    // NOLINTEND(readability-static-accessed-through-instance)

    bool deviceSpecificUpdateFunctionCalled = false;
};
