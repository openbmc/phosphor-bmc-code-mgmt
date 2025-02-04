#pragma once

#include "common/include/device.hpp"
#include "common/include/software_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <memory>

class ExampleCodeUpdater : public SoftwareManager
{
  public:
    ExampleCodeUpdater(sdbusplus::async::context& ctx);
    ExampleCodeUpdater(sdbusplus::async::context& ctx, long uniqueSuffix);

    sdbusplus::async::task<> initSingleDevice(const std::string& service,
                                              const std::string& path,
                                              SoftwareConfig& config) final;

    void init();
};

const std::string exampleConfigObjPath =
    "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleDevice";
const std::string exampleName = "ExampleSoftware";

const uint32_t exampleVendorIANA = 0x0000a015;
const std::string exampleCompatible = "com.example.compatible";

class ExampleDevice : public Device
{
  public:
    using Device::softwareCurrent;
    using Device::softwareUpdate;

    ExampleDevice(sdbusplus::async::context& ctx, const SoftwareConfig& config,
                  SoftwareManager* parent);
    ExampleDevice(sdbusplus::async::context& ctx, SoftwareManager* parent);

    // NOLINTBEGIN
    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) override;
    // NOLINTEND

    bool deviceSpecificUpdateFunctionCalled = false;

    void init();
};
