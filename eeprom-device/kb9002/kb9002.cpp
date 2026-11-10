#include "kb9002.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>
#include <iomanip>
#include <sstream>

PHOSPHOR_LOG2_USING;

std::vector<std::string> KB9002DeviceVersion::getDebugFsPaths(
    const std::string& suffix) const
{
    std::ostringstream busOss;
    std::ostringstream addrOss;

    busOss << std::setw(2) << std::setfill('0') << static_cast<int>(bus);
    addrOss << std::setw(4) << std::setfill('0') << std::hex << std::nouppercase
            << static_cast<int>(address);

    return {"/sys/kernel/debug/i2c/i2c-" + busOss.str() + "/" + busOss.str() +
            "-" + addrOss.str() + suffix};
}

std::string KB9002DeviceVersion::getVersion()
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

bool KB9002DeviceVersion::isDeviceReady()
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

    debug("Failed to get status: unable to find fw_load_status file");
    return false;
}

std::optional<SystemStateInf::HostState>
    KB9002DeviceVersion::getHostStateToQueryVersion()
{
    return SystemStateInf::HostState::Running;
}

std::optional<SystemStateInf::OsState>
    KB9002DeviceVersion::getOsStateToQueryVersion()
{
    return SystemStateInf::OsState::Standby;
}
