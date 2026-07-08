#include "pt5161l.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

PHOSPHOR_LOG2_USING;

std::string PT5161LDeviceVersion::getVersion()
{
    std::string version;
    std::ostringstream busOss;
    std::ostringstream addrOss;

    busOss << std::setw(2) << std::setfill('0') << static_cast<int>(bus);
    addrOss << std::setw(4) << std::setfill('0') << std::hex << std::nouppercase
            << static_cast<int>(address);

    // The PT5161L driver exposes the firmware version through the fw_ver node.
    // Old kernel path first, then fallback to new one.
    const std::vector<std::string> paths = {
        "/sys/kernel/debug/pt5161l/" + busOss.str() + "-" + addrOss.str() +
            "/fw_ver",
        "/sys/kernel/debug/i2c/i2c-" + busOss.str() + "/" + busOss.str() + "-" +
            addrOss.str() + "/fw_ver"};

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

        error("Failed to read version from file: {PATH}", "PATH", path);
        return version;
    }

    error("Failed to get version: unable to open file");
    return version;
}

bool PT5161LDeviceVersion::isDeviceReady()
{
    std::string status;
    std::ostringstream busOss;
    std::ostringstream addrOss;

    busOss << std::setw(2) << std::setfill('0') << static_cast<int>(bus);
    addrOss << std::setw(4) << std::setfill('0') << std::hex << std::nouppercase
            << static_cast<int>(address);

    // Old kernel path first, then fallback to new one.
    const std::vector<std::string> debugfsPaths = {
        "/sys/kernel/debug/pt5161l/" + busOss.str() + "-" + addrOss.str() +
            "/fw_load_status",
        "/sys/kernel/debug/i2c/i2c-" + busOss.str() + "/" + busOss.str() + "-" +
            addrOss.str() + "/fw_load_status"};

    for (const auto& path : debugfsPaths)
    {
        std::ifstream file(path);

        if (file && std::getline(file, status) && status == "normal")
        {
            return true;
        }
        error("Status from file: {PATH} is invalid", "PATH", path);
    }

    return false;
}

std::optional<HostPowerInf::HostState>
    PT5161LDeviceVersion::getHostStateToQueryVersion()
{
    return HostPowerInf::HostState::Running;
}
