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
            auto method = bus.new_method_call(
                "xyz.openbmc_project.Software.BMC.Updater", fwPath.c_str(),
                "org.freedesktop.DBus.Properties", "Get");
            method.append("xyz.openbmc_project.Software.RedundancyPriority",
                          "Priority");
            std::variant<uint8_t> priority;
            auto reply = bus.call(method);
            reply.read(priority);
            uint8_t thisPathPri = std::get<uint8_t>(priority);
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
