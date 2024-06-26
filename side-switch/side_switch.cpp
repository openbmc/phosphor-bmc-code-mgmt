#include "side_switch.hpp"

#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <exception>
#include <string>
#include <thread>
#include <variant>
#include <vector>

PHOSPHOR_LOG2_USING;

bool sideSwitchNeeded(sdbusplus::bus_t& bus)
{
    std::string fwRunningVersionPath;
    uint8_t fwRunningPriority = 0;

    // Get active image
    try
    {
        std::vector<std::string> paths =
            utils::getProperty<std::vector<std::string>>(
                bus, "/xyz/openbmc_project/software/functional",
                "xyz.openbmc_project.Association", "endpoints");
        if (paths.size() != 1)
        {
            info("side-switch only supports BMC-purpose image systems");
            return (false);
        }
        fwRunningVersionPath = paths[0];
        info("Running firmware version path is {FW_PATH}", "FW_PATH",
             fwRunningVersionPath);
    }
    catch (const std::exception& e)
    {
        error("failed to retrieve active firmware version: {ERROR}", "ERROR",
              e);
        return (false);
    }

    // Check if active image has highest priority (0)
    try
    {
        fwRunningPriority = utils::getProperty<uint8_t>(
            bus, fwRunningVersionPath,
            "xyz.openbmc_project.Software.RedundancyPriority", "Priority");
        info("Running firmware version priority is {FW_PRIORITY}",
             "FW_PRIORITY", fwRunningPriority);
    }
    catch (const std::exception& e)
    {
        error("failed to read priority from active image: {ERROR}", "ERROR", e);
        return (false);
    }

    // If running at highest priority (0) then no side switch needed
    if (fwRunningPriority == 0)
    {
        info("Running image is at priority 0, no side switch needed");
        return (false);
    }

    // Need to check if any other BMC images on system have a higher priority
    std::vector<std::string> allSoftwarePaths;
    try
    {
        auto method = bus.new_method_call("xyz.openbmc_project.ObjectMapper",
                                          "/xyz/openbmc_project/object_mapper",
                                          "xyz.openbmc_project.ObjectMapper",
                                          "GetSubTreePaths");
        method.append("/xyz/openbmc_project/software");
        method.append(0); // Depth 0 to search all
        method.append(
            std::vector<std::string>({"xyz.openbmc_project.Software.Version"}));
        auto reply = bus.call(method);
        reply.read(allSoftwarePaths);
        if (allSoftwarePaths.size() <= 1)
        {
            info("only 1 image present in flash so no side switch needed");
            return (false);
        }
    }
    catch (const std::exception& e)
    {
        error("failed to retrieve all firmware versions: {ERROR}", "ERROR", e);
        return (false);
    }

    // Cycle through all firmware images looking for a BMC version that
    // has a higher priority then our running image
    for (auto& fwPath : allSoftwarePaths)
    {
        if (fwPath == fwRunningVersionPath)
        {
            info("{FW_PATH} is the running image, skip", "FW_PATH", fwPath);
            continue;
        }
        try
        {
            uint8_t thisPathPri = utils::getProperty<uint8_t>(
                bus, fwPath, "xyz.openbmc_project.Software.RedundancyPriority",
                "Priority");

            if (thisPathPri < fwRunningPriority)
            {
                info(
                    "{FW_PATH} has a higher priority, {FW_PRIORITY}, then running priority",
                    "FW_PATH", fwPath, "FW_PRIORITY", thisPathPri);
                return (true);
            }
        }
        catch (const std::exception& e)
        {
            // This could just be a host firmware image, just keep going
            info("failed to read a BMC priority from {FW_PATH}: {ERROR}",
                 "FW_PATH", fwPath, "ERROR", e);
            continue;
        }
    }

    return (false);
}

bool powerOffSystem(sdbusplus::bus_t& bus)
{
    try
    {
        utils::PropertyValue chassOff =
            "xyz.openbmc_project.State.Chassis.Transition.Off";
        utils::setProperty(bus, "/xyz/openbmc_project/state/chassis0",
                           "xyz.openbmc_project.State.Chassis",
                           "RequestedPowerTransition", chassOff);
    }
    catch (const std::exception& e)
    {
        error("chassis off request failed: {ERROR}", "ERROR", e);
        return (false);
    }

    // Now just wait for host and power to turn off
    // Worst case is a systemd service hangs in power off for 2 minutes so
    // take that and double it to avoid any timing issues. The user has
    // requested we switch to the other side, so a lengthy delay is warranted
    // if needed. On most systems the power off takes 5-15 seconds.
    for (int i = 0; i < 240; i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        try
        {
            // First wait for host to be off
            auto currentHostState = utils::getProperty<std::string>(
                bus, "/xyz/openbmc_project/state/host0",
                "xyz.openbmc_project.State.Host", "CurrentHostState");

            if (currentHostState ==
                "xyz.openbmc_project.State.Host.HostState.Off")
            {
                info("host is off");
            }
            else
            {
                continue;
            }

            // Then verify chassis power is off
            auto currentPwrState = utils::getProperty<std::string>(
                bus, "/xyz/openbmc_project/state/chassis0",
                "xyz.openbmc_project.State.Chassis", "CurrentPowerState");

            if (currentPwrState ==
                "xyz.openbmc_project.State.Chassis.PowerState.Off")
            {
                info("chassis power is off");
                return (true);
            }
            else
            {
                continue;
            }
        }
        catch (const std::exception& e)
        {
            error("reading chassis power state failed: {ERROR}", "ERROR", e);
            return (false);
        }
    }
    error("timeout waiting for chassis power to turn off");
    return (false);
}

bool setAutoPowerRestart(sdbusplus::bus_t& bus)
{
    try
    {
        // Set the one-time power on policy to AlwaysOn so system auto boots
        // after BMC reboot
        utils::PropertyValue restorePolicyOn =
            "xyz.openbmc_project.Control.Power.RestorePolicy.Policy.AlwaysOn";

        utils::setProperty(
            bus,
            "/xyz/openbmc_project/control/host0/power_restore_policy/one_time",
            "xyz.openbmc_project.Control.Power.RestorePolicy",
            "PowerRestorePolicy", restorePolicyOn);
    }
    catch (const std::exception& e)
    {
        error("setting power policy to always on failed: {ERROR}", "ERROR", e);
        return (false);
    }
    info("RestorePolicy set to AlwaysOn");
    return (true);
}

bool rebootTheBmc(sdbusplus::bus_t& bus)
{
    try
    {
        utils::PropertyValue bmcReboot =
            "xyz.openbmc_project.State.BMC.Transition.Reboot";

        utils::setProperty(bus, "/xyz/openbmc_project/state/bmc0",
                           "xyz.openbmc_project.State.BMC",
                           "RequestedBMCTransition", bmcReboot);
    }
    catch (const std::exception& e)
    {
        error("rebooting the bmc failed: {ERROR}", "ERROR", e);
        return (false);
    }
    info("BMC reboot initiated");
    return (true);
}

int main()
{
    info("Checking for side switch reboot");

    auto bus = sdbusplus::bus::new_default();

    if (!sideSwitchNeeded(bus))
    {
        info("Side switch not needed");
        return 0;
    }

    if (!powerOffSystem(bus))
    {
        error("unable to power off chassis");
        return 0;
    }

    if (!setAutoPowerRestart(bus))
    {
        error("unable to set the auto power on restart policy");
        // system has been powered off, best to at least continue and
        // switch to new firmware image so continue
    }

    if (!rebootTheBmc(bus))
    {
        error("unable to reboot the BMC");
        // Return invalid rc to trigger systemd target recovery and appropriate
        // error logging
        return -1;
    }

    return 0;
}
