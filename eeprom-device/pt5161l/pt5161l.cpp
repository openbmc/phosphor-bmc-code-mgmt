#include "pt5161l.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

PHOSPHOR_LOG2_USING;

std::vector<std::string> PT5161LDeviceVersion::getDebugFsPaths(
    const std::string& suffix) const
{
    std::ostringstream busOss;
    std::ostringstream addrOss;

    busOss << std::setw(2) << std::setfill('0') << static_cast<int>(bus);
    addrOss << std::setw(4) << std::setfill('0') << std::hex << std::nouppercase
            << static_cast<int>(address);

    /* The PT5161L driver exposes the firmware version through the fw_ver node.
     * The debugfs path changed starting from Linux kernel v6.18.
     * Try the legacy path first, then fall back to the new path.
     */
    return {"/sys/kernel/debug/pt5161l/" + busOss.str() + "_" + addrOss.str() +
                suffix,

            "/sys/kernel/debug/i2c/i2c-" + busOss.str() + "/" + busOss.str() +
                "-" + addrOss.str() + suffix};
}

std::string PT5161LDeviceVersion::getVersion()
{
    std::string version;
    const std::vector<std::string> paths = getDebugFsPaths("/fw_ver");

    for (const auto& path : paths)
    {
        std::ifstream file(path);

        if (!file)
        {
            continue;
        }

        if (std::getline(file, version) && !version.empty())
        {
            return version;
        }

        return version;
    }

    error("Failed to get version: unable to find fw_ver file");
    return version;
}

bool PT5161LDeviceVersion::isDeviceReady()
{
    std::string status;
    const std::vector<std::string> debugfsPaths =
        getDebugFsPaths("/fw_load_status");

    for (const auto& path : debugfsPaths)
    {
        std::ifstream file(path);

        if (file && std::getline(file, status) && status == "normal")
        {
            return true;
        }
    }

    error("Failed to get status: unable to find fw_load_status file");
    return false;
}

std::optional<HostPowerInf::HostState>
    PT5161LDeviceVersion::getHostStateToQueryVersion()
{
    return HostPowerInf::HostState::Running;
}
