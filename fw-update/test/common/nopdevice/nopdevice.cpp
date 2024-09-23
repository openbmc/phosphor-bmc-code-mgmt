#include "nopdevice.hpp"

#include "fw-update/common/include/device.hpp"
#include "fw-update/common/include/device_config.hpp"
#include "fw-update/common/include/software_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <memory>

NopCodeUpdater::NopCodeUpdater(sdbusplus::async::context& ctx) :
    SoftwareManager(ctx, "Nop", true)
{}

// NOLINTBEGIN
sdbusplus::async::task<> NopCodeUpdater::getInitialConfigurationSingleDevice(
    const std::string& /*unused*/, DeviceConfig& /*unused*/)
// NOLINTEND
{
    co_return;
}

NopDevice::NopDevice(sdbusplus::async::context& ctx, const DeviceConfig& config,
                     SoftwareManager* parent) :
    Device(ctx, true, config, parent)
{}
NopDevice::NopDevice(sdbusplus::async::context& ctx, SoftwareManager* parent) :
    Device(ctx, true,
           DeviceConfig(exampleVendorIANA, exampleCompatible,
                        exampleConfigObjPath),
           parent)
{}

// NOLINTBEGIN
sdbusplus::async::task<bool> NopDevice::deviceSpecificUpdateFunction(
    const uint8_t* /*unused*/, size_t /*unused*/,
    std::unique_ptr<SoftwareActivationProgress>& /*unused*/)
// NOLINTEND
{
    deviceSpecificUpdateFunctionCalled = true;

    co_return true;
}

std::string NopDevice::getObjPathConfig()
{
    return this->config.objPathConfig;
}
