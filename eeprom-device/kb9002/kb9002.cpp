#include "kb9002.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>
#include <iomanip>
#include <sstream>

PHOSPHOR_LOG2_USING;

std::string KB9002DeviceVersion::getVersion()
{
    std::string version;
    std::ostringstream busOss;
    std::ostringstream addrOss;

    busOss << std::setw(2) << std::setfill('0') << static_cast<int>(bus);
    addrOss << std::setw(4) << std::setfill('0') << std::hex << std::nouppercase
            << static_cast<int>(address);

    std::string path = "/sys/kernel/debug/i2c/i2c-" + busOss.str() + "/" +
                       busOss.str() + "-" + addrOss.str() + "/fw_ver";

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

bool KB9002DeviceVersion::isDeviceReady()
{
    std::string status;
    std::ostringstream busOss;
    std::ostringstream addrOss;

    busOss << std::setw(2) << std::setfill('0') << static_cast<int>(bus);
    addrOss << std::setw(4) << std::setfill('0') << std::hex << std::nouppercase
            << static_cast<int>(address);

    std::string fw_load_status =
        "/sys/kernel/debug/i2c/i2c-" + busOss.str() + "/" + busOss.str() + "-" +
        addrOss.str() + "/fw_load_status";

    std::ifstream file(fw_load_status);

    if (file && std::getline(file, status) && status == "normal")
    {
        return true;
    }

    debug("Status from file: {PATH} is invalid: {STATUS}", "PATH",
          fw_load_status, "STATUS", status);

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
