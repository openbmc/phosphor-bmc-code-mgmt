#include "e810_device.hpp"

#include "mctp/endpoint.hpp"
#include "pldm/firmware_parameters.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software;

namespace
{

constexpr auto e810UUID = "5af04860-05df-11e4-af79-000100000000";

} // namespace

E810Device::E810Device(sdbusplus::async::context& ctx,
                       uint64_t inSpiControllerIndex, uint64_t inSpiDeviceIndex,
                       bool inDryRun,
                       const std::vector<std::string>& inGpioLines,
                       const std::vector<bool>& inGpioValues,
                       SoftwareConfig& inConfig, SoftwareManager* inParent) :
    SPIDevice(ctx, inSpiControllerIndex, inSpiDeviceIndex, inDryRun,
              inGpioLines, inGpioValues, inConfig, inParent, flashLayoutFlat,
              flashToolFlashcp),
    transport(ctx)
{
    debug("E810 device initialized");

    // Version discovery runs in the background so neither service startup
    // nor getVersion() ever waits on MCTP.
    ctx.spawn(refreshVersion());
}

std::string E810Device::getVersion()
{
    return version;
}

sdbusplus::async::task<> E810Device::refreshVersion()
{
    auto eid = co_await mctp::waitForEndpoint(ctx, e810UUID);

    auto newVersion = co_await pldm::getActiveFirmwareVersion(transport, eid);
    if (!newVersion.has_value())
    {
        co_return;
    }

    version = newVersion.value();

    if (softwareCurrent)
    {
        softwareCurrent->setVersion(version,
                                    SoftwareVersion::VersionPurpose::Other);
    }

    debug("E810 version is {VERSION}", "VERSION", version);
    co_return;
}
