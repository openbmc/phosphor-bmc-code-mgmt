#include "bios_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

#include <fstream>
PHOSPHOR_LOG2_USING;
using namespace phosphor::software;
using namespace phosphor::software::host_power;

static enum FlashTool getBiosFlashTool(const std::string& configType)
{
    if (configType == "IntelHostSPIFlash")
    {
        return flashToolFlashrom;
    }
    return flashToolFlashcp;
}

BIOSDevice::BIOSDevice(sdbusplus::async::context& ctx,
                       uint64_t spiControllerIndex, uint64_t spiDeviceIndex,
                       bool dryRun, const std::vector<std::string>& gpioLinesIn,
                       const std::vector<bool>& gpioValuesIn,
                       SoftwareConfig& config, SoftwareManager* parent) :
    SPIDevice(ctx, spiControllerIndex, spiDeviceIndex, dryRun, gpioLinesIn,
              gpioValuesIn, config, parent, flashLayoutFlat,
              getBiosFlashTool(config.configType)),
    NotifyWatchIntf(ctx, "/var/bios/"), versionDirPath("/var/bios/"),
    versionFilename("host0_bios_version.txt")
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
            error("Failed to open BIOS version file at {PATH}", "PATH",
                  fullPath);
            version = versionUnknown;
        }
    }
    catch (std::exception& e)
    {
        error("Failed to get BIOS version with {ERROR}", "ERROR", e.what());
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
        error(
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
        error("error changing host power state");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> BIOSDevice::postUpdate()
{
    const bool powerstate_restore =
        co_await HostPower::setState(ctx, prevPowerstate);
    if (!powerstate_restore)
    {
        error("error restoring host power state");
        co_return false;
    }

    co_return true;
}
