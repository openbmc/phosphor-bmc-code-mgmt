#include "bios_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

#include <fstream>
#include <string>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software;
using namespace phosphor::software::host_power;

namespace
{

constexpr auto biosVersionDirPath = "/var/bios/";
constexpr auto biosVersionFilename = "host0_bios_version.txt";

enum FlashTool getBiosFlashTool(const std::string& configType)
{
    if (configType == "IntelHostSPIFlash")
    {
        return flashToolFlashrom;
    }
    return flashToolFlashcp;
}

} // namespace

BIOSDevice::BIOSDevice(sdbusplus::async::context& ctx,
                       uint64_t spiControllerIndex, uint64_t spiDeviceIndex,
                       bool dryRun, const std::vector<std::string>& gpioLinesIn,
                       const std::vector<bool>& gpioValuesIn,
                       SoftwareConfig& config, SoftwareManager* parent) :
    SPIDevice(ctx, spiControllerIndex, spiDeviceIndex, dryRun, gpioLinesIn,
              gpioValuesIn, config, parent, flashLayoutFlat,
              getBiosFlashTool(config.configType)),
    versionWatch(ctx, biosVersionDirPath, *this)
{
    ctx.spawn(versionWatch.readNotifyAsync());
}

void BIOSDevice::refreshVersion()
{
    if (softwareCurrent)
    {
        softwareCurrent->setVersion(getVersion(),
                                    SoftwareVersion::VersionPurpose::Host);
    }
}

sdbusplus::async::task<> BIOSDevice::VersionWatch::processUpdate(
    std::string inVersionFilename)
{
    if (inVersionFilename != biosVersionFilename)
    {
        error(
            "Update version file name '{NAME}' (!= '{EXPECTED}') is not expected",
            "NAME", inVersionFilename, "EXPECTED", biosVersionFilename);
        co_return;
    }

    owner.refreshVersion();

    co_return;
}

std::string BIOSDevice::getVersion()
{
    std::string version = versionUnknown;
    const std::string fullPath =
        std::string(biosVersionDirPath) + biosVersionFilename;

    try
    {
        std::ifstream versionFile(fullPath);
        if (versionFile.is_open())
        {
            versionFile >> version;
        }
        else
        {
            error("Failed to open BIOS version file at {PATH}", "PATH",
                  fullPath);
        }
    }
    catch (const std::exception& e)
    {
        error("Failed to get BIOS version with {ERROR}", "ERROR", e.what());
    }

    if (version.empty())
    {
        version = versionUnknown;
    }

    return version;
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
    const bool powerstateRestored =
        co_await HostPower::setState(ctx, prevPowerstate);
    if (!powerstateRestored)
    {
        error("error restoring host power state");
        co_return false;
    }

    co_return true;
}
