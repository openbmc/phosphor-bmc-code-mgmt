#include "nopdevice.hpp"

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

NopCodeUpdater::NopCodeUpdater(sdbusplus::async::context& io) :
    SoftwareManager(io, "Nop", true, false)
{}

NopDevice::NopDevice(sdbusplus::async::context& io, SoftwareManager* parent) :
    Device(io, true, exampleVendorIANA, exampleCompatible, exampleConfigObjPath,
           parent)
{}

// NOLINTBEGIN
sdbusplus::async::task<bool> NopDevice::deviceSpecificUpdateFunction(
    const uint8_t* image, size_t image_size,
    std::unique_ptr<SoftwareActivationProgress>& activationProgress)
// NOLINTEND
{
    (void)image;
    (void)image_size;
    (void)activationProgress;

    deviceSpecificUpdateFunctionCalled = true;

    co_return true;
}

std::string NopDevice::getObjPathConfig()
{
    return this->objPathConfig;
}

int NopDevice::getUpdateTimerDelaySeconds()
{
    return 0;
}
