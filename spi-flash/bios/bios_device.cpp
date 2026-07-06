#include "bios_device.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>

using namespace phosphor::software;
using namespace phosphor::software::host_power;

BIOSDevice::BIOSDevice(
    sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
    uint64_t spiDeviceIndex, bool dryRun,
    const std::vector<std::string>& gpioLinesIn,
    const std::vector<bool>& gpioValuesIn, SoftwareConfig& config,
    SoftwareManager* parent, enum FlashLayout layout, enum FlashTool tool,
    const std::string& versionDirPath, const std::string& versionFilename) :
    SPIDevice(ctx, spiControllerIndex, spiDeviceIndex, dryRun, gpioLinesIn,
              gpioValuesIn, config, parent, layout, tool),
    NotifyWatchIntf(ctx, versionDirPath), versionDirPath(versionDirPath),
    versionFilename(versionFilename)
{
    ctx.spawn(readNotifyAsync());
}

std::string BIOSDevice::getVersion()
{
    const std::string versionUnknown = "Unknown";
    std::string version = versionUnknown;
    std::string fullPath = versionDirPath + versionFilename;

    try
    {
        std::ifstream config(fullPath);
        if (config.is_open())
        {
            config >> version;
        }
        else
        {
            lg2::error("Failed to open BIOS version file at {PATH}", "PATH",
                       fullPath);
            version = versionUnknown;
        }
    }
    catch (std::exception& e)
    {
        lg2::error("Failed to get BIOS version with {ERROR}", "ERROR",
                   e.what());
        version = versionUnknown;
    }

    if (version.empty())
    {
        version = versionUnknown;
    }

    return version;
}

sdbusplus::async::task<> BIOSDevice::processUpdate(std::string versionFileName)
{
    if (versionFilename != versionFileName)
    {
        lg2::error(
            "Update config file name '{NAME}' (!= '{EXPECTED}') is not expected",
            "NAME", versionFileName, "EXPECTED", versionFilename);
        co_return;
    }

    if (softwareCurrent)
    {
        softwareCurrent->setVersion(getVersion(),
                                    SoftwareVersion::VersionPurpose::Host);
    }

    co_return;
}

sdbusplus::async::task<bool> BIOSDevice::preUpdate()
{
    prevPowerstate = co_await HostPower::getState(ctx);

    if (prevPowerstate != stateOn && prevPowerstate != stateOff)
    {
        co_return false;
    }

    bool success = co_await HostPower::setState(ctx, stateOff);
    if (!success)
    {
        lg2::error("error changing host power state");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> BIOSDevice::postUpdate()
{
    if (softwarePending)
    {
        softwarePending->setActivationBlocksTransition(false);
        lg2::debug("ActivationBlocksTransition lifted for host power restore");
    }

    const bool powerstate_restore =
        co_await HostPower::setState(ctx, prevPowerstate);
    if (!powerstate_restore)
    {
        lg2::error("error restoring host power state");
        co_return false;
    }

    co_return true;
}
