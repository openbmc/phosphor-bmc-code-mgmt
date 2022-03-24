#include "side_switch.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>

#include <exception>
#include <string>
#include <variant>
#include <vector>

PHOSPHOR_LOG2_USING;

bool sideSwitchNeeded()
{
    auto bus = sdbusplus::bus::new_default();
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

int main()
{
    info("Checking for side switch reboot");

    if (!sideSwitchNeeded())
    {
        info("Side switch not needed");
        return 0;
    }

    // TODO - Future commits in series to fill in rest of logic
}
