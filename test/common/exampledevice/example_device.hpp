#pragma once

#include "common/include/device.hpp"
#include "common/include/software_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

namespace phosphor::software::example_device
{

class ExampleDevice;

class ExampleCodeUpdater : public phosphor::software::manager::SoftwareManager
{
  public:
    // @param createDevice  create an ExampleDevice. Prerequisite for param
    // 'swVersion'.
    // @param swVersion     if this is nullptr, do not create the software
    // version.
    ExampleCodeUpdater(sdbusplus::async::context& ctx, bool createDevice,
                       const char* swVersion);

    ExampleCodeUpdater(sdbusplus::async::context& ctx);

    std::optional<std::string> getBusName();

    std::unique_ptr<ExampleDevice>& getDevice();

    sdbusplus::async::task<bool> initDevice(const std::string& service,
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

class ExampleSoftware : public Software
{
  public:
    using Software::createInventoryAssociation;
    using Software::objectPath;
    ExampleSoftware(sdbusplus::async::context& ctx, ExampleDevice& parent);
};

class ExampleDevice : public Device
{
  public:
    using Device::softwarePending;
    std::unique_ptr<ExampleSoftware> softwareCurrent;

    static SoftwareConfig defaultConfig;

    ExampleDevice(sdbusplus::async::context& ctx,
                  phosphor::software::manager::SoftwareManager* parent,
                  const SoftwareConfig& config = defaultConfig);

    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) override;

    bool deviceSpecificUpdateFunctionCalled = false;
};

} // namespace phosphor::software::example_device
