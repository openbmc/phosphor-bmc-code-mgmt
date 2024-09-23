#pragma once

#include "fw-update/common/include/device.hpp"
#include "fw-update/common/include/software_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <memory>

class NopCodeUpdater : public SoftwareManager
{
  public:
    NopCodeUpdater(sdbusplus::async::context& ctx);
    NopCodeUpdater(sdbusplus::async::context& ctx, long uniqueSuffix);

    std::set<std::string> getEMConfigTypes() override;

    sdbusplus::async::task<> getInitialConfigurationSingleDevice(
        const std::string& service, DeviceConfig& config) final;
};

const std::string exampleConfigObjPath =
    "/xyz/openbmc_project/inventory/system/board/Tyan_S5549_Baseboard/HostSPIFlash";

const uint32_t exampleVendorIANA = 0x0000a015;
const std::string exampleCompatible = "com.example.compatible";

class NopDevice : public Device
{
  public:
    NopDevice(sdbusplus::async::context& ctx, const DeviceConfig& config,
              SoftwareManager* parent);
    NopDevice(sdbusplus::async::context& ctx, SoftwareManager* parent);

    // NOLINTBEGIN
    sdbusplus::async::task<bool> deviceSpecificUpdateFunction(
        const uint8_t* image, size_t image_size,
        std::unique_ptr<SoftwareActivationProgress>& activationProgress)
        override;
    // NOLINTEND

    sdbusplus::async::task<std::string> getInventoryItemObjectPath() override;

    std::string getObjPathConfig();

    bool deviceSpecificUpdateFunctionCalled = false;
};
