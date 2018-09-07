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

void storeToFile(std::string versionId, uint8_t priority)
{
    if (!fs::is_directory(PERSIST_DIR))
    {
        fs::create_directories(PERSIST_DIR);
    }
    std::string path = PERSIST_DIR + versionId;

    std::ofstream os(path.c_str());
    cereal::JSONOutputArchive oarchive(os);
    oarchive(cereal::make_nvp("priority", priority));
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

} // namespace updater
} // namespace software
} // namespace phosphor
