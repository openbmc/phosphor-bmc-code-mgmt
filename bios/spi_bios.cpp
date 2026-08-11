#include "spi_bios.hpp"

#include "common/include/host_power.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

#include <fstream>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software::host_power;

SPIBIOS::SPIBIOS(sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
                 uint64_t spiDeviceIndex, bool dryRun, GPIOGroup&& muxGPIO,
                 SoftwareConfig& config, SoftwareManager* parent,
                 enum FlashLayout layout, enum FlashTool tool,
                 const std::string& versionDirPath) :
    SPIDevice(ctx, spiControllerIndex, spiDeviceIndex, dryRun,
              std::move(muxGPIO), config, parent, layout, tool),
    NotifyWatchIntf(ctx, versionDirPath)
{
    ctx.spawn(readNotifyAsync());
}

std::string SPIBIOS::getVersion() const
{
    std::string version{};
    try
    {
        std::ifstream config(biosVersionPath);

        config >> version;
    }
    catch (std::exception& e)
    {
        error("Failed to get version with {ERROR}", "ERROR", e.what());
        version = versionUnknown;
    }

    if (version.empty())
    {
        version = versionUnknown;
    }

    return version;
}

sdbusplus::async::task<bool> SPIBIOS::updateDevice(const uint8_t* image,
                                                   size_t image_size)
{
    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch)
    auto prevPowerstate = co_await HostPower::getState(ctx);

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

    success = co_await SPIDevice::updateDevice(image, image_size);

    // lift the activation blocks transition to prevent rejection of the
    // host power state restore request.
    if (softwarePending)
    {
        softwarePending->setActivationBlocksTransition(false);
        debug("ActivationBlocksTransition lifted for host power restore");
    }

    // restore the previous powerstate
    const bool powerstate_restore =
        co_await HostPower::setState(ctx, prevPowerstate);
    if (!powerstate_restore)
    {
        error("error changing host power state");
        co_return false;
    }

    // return value here is only describing if we successfully wrote to the
    // SPI flash. Restoring powerstate can still fail.
    co_return success;
}

auto SPIBIOS::processUpdate(std::string versionFileName)
    -> sdbusplus::async::task<>
{
    if (biosVersionFilename != versionFileName)
    {
        error(
            "Update config file name '{NAME}' (!= '{EXPECTED}') is not expected",
            "NAME", versionFileName, "EXPECTED", biosVersionFilename);
        co_return;
    }

    if (softwareCurrent)
    {
        softwareCurrent->setVersion(getVersion(),
                                    SoftwareVersion::VersionPurpose::Host);
    }

    co_return;
}
