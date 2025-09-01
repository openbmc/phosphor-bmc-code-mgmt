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
    std::string status;
    std::ostringstream busOss;
    std::ostringstream addrOss;
    bool isStatusNormal = false;
    constexpr int maxRetries = 15;
    constexpr auto retryDelay = std::chrono::seconds(2);

    busOss << std::setw(2) << std::setfill('0') << static_cast<int>(bus);
    addrOss << std::setw(4) << std::setfill('0') << std::hex << std::nouppercase
            << static_cast<int>(address);

    std::string fw_load_status = "/sys/kernel/debug/pt5161l/" + busOss.str() +
                                 "-" + addrOss.str() + "/fw_load_status";

    for (int i = 0; i < maxRetries; ++i)
    {
        std::ifstream file(fw_load_status);

        if (file && std::getline(file, status) && status == "normal")
        {
            isStatusNormal = true;
            break;
        }

        error("Status from file: {PATH} is invalid: {STATUS}, retrying...",
              "PATH", fw_load_status, "STATUS", status);

        std::this_thread::sleep_for(retryDelay);
    }

    if (!isStatusNormal)
    {
        error("Failed to get version: firmware load status is not normal");
        return version;
    }

    // The PT5161L driver exposes the firmware version through the fw_ver node
    std::string path = "/sys/kernel/debug/pt5161l/" + busOss.str() + "-" +
                       addrOss.str() + "/fw_ver";

    std::ifstream file(path);
    if (!file)
    {
        error("Failed to get version: unable to open file: {PATH}", "PATH",
              path);
        return version;
    }

    if (!std::getline(file, version) || version.empty())
    {
        error("Failed to read version from file: {PATH}", "PATH", path);
    }

    return version;
}

std::optional<HostPowerInf::HostState>
    PT5161LDeviceVersion::getHostStateToQueryVersion()
{
    return HostPowerInf::HostState::Running;
}
