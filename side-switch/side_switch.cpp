#include "side_switch.hpp"

#include <phosphor-logging/lg2.hpp>

#include <exception>
#include <string>
#include <thread>
#include <variant>
#include <vector>

PHOSPHOR_LOG2_USING;

bool sideSwitchNeeded(sdbusplus::bus::bus& bus)
{

    std::string fwRunningVersionPath;
    uint8_t fwRunningPriority = 0;

    // Get active image
    try
    {
        auto method =
            bus.new_method_call("xyz.openbmc_project.ObjectMapper",
                                "/xyz/openbmc_project/software/functional",
                                "org.freedesktop.DBus.Properties", "Get");
        method.append("xyz.openbmc_project.Association", "endpoints");
        std::variant<std::vector<std::string>> paths;
        auto reply = bus.call(method);
        reply.read(paths);
        if (std::get<std::vector<std::string>>(paths).size() != 1)
        {
            info("side-switch only supports single image systems");
            return (false);
        }
        fwRunningVersionPath = std::get<std::vector<std::string>>(paths)[0];
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
        auto method =
            bus.new_method_call("xyz.openbmc_project.Software.BMC.Updater",
                                fwRunningVersionPath.c_str(),
                                "org.freedesktop.DBus.Properties", "Get");
        method.append("xyz.openbmc_project.Software.RedundancyPriority",
                      "Priority");
        std::variant<uint8_t> priority;
        auto reply = bus.call(method);
        reply.read(priority);
        fwRunningPriority = std::get<uint8_t>(priority);
        info("Running firmware version priority is {FW_PRIORITY}",
             "FW_PRIORITY", fwRunningPriority);
    }
    catch (const std::exception& e)
    {
        error("failed to read priority from active image: {ERROR}", "ERROR", e);
        return (false);
    }

    if (fwRunningPriority != 0)
    {
        info("Running image is at priority {FW_PRIORITY}, execute side switch",
             "FW_PRIORITY", fwRunningPriority);
        return (true);
    }

    return (false);
}

bool powerOffSystem(sdbusplus::bus::bus& bus)
{

    try
    {
        auto method =
            bus.new_method_call("xyz.openbmc_project.State.Chassis",
                                "/xyz/openbmc_project/state/chassis0",
                                "org.freedesktop.DBus.Properties", "Set");

        method.append("xyz.openbmc_project.State.Chassis",
                      "RequestedPowerTransition",
                      std::variant<std::string>{
                          "xyz.openbmc_project.State.Chassis.Transition.Off"});
        auto reply = bus.call(method);
    }
    catch (const std::exception& e)
    {
        error("chassis off request failed: {ERROR}", "ERROR", e);
        return (false);
    }

    // Now just wait for power to turn off
    for (int i = 0; i < 30; i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        try
        {
            auto method =
                bus.new_method_call("xyz.openbmc_project.State.Chassis",
                                    "/xyz/openbmc_project/state/chassis0",
                                    "org.freedesktop.DBus.Properties", "Get");
            method.append("xyz.openbmc_project.State.Chassis",
                          "CurrentPowerState");
            std::variant<std::string> currentPwrState;
            auto reply = bus.call(method);
            reply.read(currentPwrState);
            if (std::get<std::string>(currentPwrState) ==
                "xyz.openbmc_project.State.Chassis.PowerState.Off")
            {
                info("chassis power is off");
                return (true);
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

    // TODO - Future commits in series to fill in rest of logic
}
