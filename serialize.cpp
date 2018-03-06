#include "config.h"
#include <experimental/filesystem>
#include <cereal/archives/json.hpp>
#include <fstream>
#include <ctime>
#include <unistd.h>
#include "serialize.hpp"
#include <sdbusplus/server.hpp>
#include <phosphor-logging/log.hpp>

namespace phosphor
{
namespace software
{
namespace updater
{

namespace fs = std::experimental::filesystem;

using namespace phosphor::logging;

void storeToFile(std::string versionId, uint8_t priority)
{
    auto bus = sdbusplus::bus::new_default();

    if (!fs::is_directory(PERSIST_DIR))
    {
        fs::create_directories(PERSIST_DIR);
    }
    std::string path = PERSIST_DIR + versionId;

    std::ofstream os(path.c_str());
    cereal::JSONOutputArchive oarchive(os);
    oarchive(cereal::make_nvp("priority", priority));

    std::string serviceFile = "obmc-flash-bmc-setenv@" + versionId + "\\x3d" +
                              std::to_string(priority) + ".service";
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(serviceFile, "replace");
    bus.call_noreply(method);

    // On average it takes 1-2 seconds for the service to complete.
    // Therefore timeout is set to 3x average completion time.
    waitForServiceFile(serviceFile, 6);
}

bool restoreFromFile(std::string versionId, uint8_t& priority)
{
    std::string path = PERSIST_DIR + versionId;
    if (fs::exists(path))
    {
        std::ifstream is(path.c_str(), std::ios::in);
        try
        {
            cereal::JSONInputArchive iarchive(is);
            iarchive(cereal::make_nvp("priority", priority));
            return true;
        }
        catch (cereal::RapidJSONException& e)
        {
            fs::remove(path);
        }
    }

    // Find the mtd device "u-boot-env" to retrieve the environment variables
    std::ifstream mtdDevices("/proc/mtd");
    std::string device, devicePath;

    try
    {
        while (std::getline(mtdDevices, device))
        {
            if (device.find("u-boot-env") != std::string::npos)
            {
                devicePath = "/dev/" + device.substr(0, device.find(':'));
                break;
            }
        }

        if (!devicePath.empty())
        {
            std::ifstream input(devicePath.c_str());
            std::string envVars;
            std::getline(input, envVars);

            std::string versionVar = versionId + "=";
            auto varPosition = envVars.find(versionVar);

            if (varPosition != std::string::npos)
            {
                // Grab the environment variable for this versionId. These
                // variables follow the format "versionId=priority\0"
                auto var = envVars.substr(varPosition);
                priority = std::stoi(var.substr(versionVar.length()));
                return true;
            }
        }
    }
    catch (const std::exception& e)
    {
    }

    return false;
}

void removeFile(std::string versionId)
{
    std::string path = PERSIST_DIR + versionId;
    if (fs::exists(path))
    {
        fs::remove(path);
    }
}

void waitForServiceFile(const std::string& serviceFile, int timeout)
{
    auto bus = sdbusplus::bus::new_default();

    std::time_t start = time(0);
    std::time_t end = time(0);

    while (end - start < timeout)
    {
        auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                          SYSTEMD_INTERFACE, "GetUnit");
        method.append(serviceFile);
        auto result = bus.call(method);

        if (result.is_method_error())
        {
            return;
        }

        usleep(1000);
        end = time(0);
    }

    log<level::ERR>("Service file timed out!");
}

} // namespace phosphor
} // namespace software
} // namespace openpower
