#include "config.h"

#include "serialize.hpp"

#include <cereal/archives/json.hpp>
#include <experimental/filesystem>
#include <fstream>
#include <sdbusplus/server.hpp>

namespace phosphor
{
namespace software
{
namespace updater
{

namespace fs = std::experimental::filesystem;

const std::string priorityName = "priority";

void storePriorityToFile(std::string versionId, uint8_t priority)
{
    auto path = fs::path(PERSIST_DIR) / versionId;
    if (!fs::exists(path))
    {
        fs::create_directories(path);
    }
    path = path / priorityName;

    std::ofstream os(path.c_str());
    cereal::JSONOutputArchive oarchive(os);
    oarchive(cereal::make_nvp(priorityName, priority));
}

bool restorePriorityFromFile(std::string versionId, uint8_t& priority)
{
    auto path = fs::path(PERSIST_DIR) / versionId / priorityName;
    if (fs::exists(path))
    {
        std::ifstream is(path.c_str(), std::ios::in);
        try
        {
            cereal::JSONInputArchive iarchive(is);
            iarchive(cereal::make_nvp(priorityName, priority));
            return true;
        }
        catch (cereal::Exception& e)
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
    auto path = fs::path(PERSIST_DIR) / versionId;
    if (fs::exists(path))
    {
        fs::remove_all(path);
    }
}

} // namespace updater
} // namespace software
} // namespace phosphor
