#include "config.h"

#include "serialize.hpp"

#include <cereal/archives/json.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/server.hpp>

#include <filesystem>
#include <fstream>
#include <system_error>

namespace phosphor
{
namespace software
{
namespace updater
{

PHOSPHOR_LOG2_USING;
namespace fs = std::filesystem;

const std::string priorityName = "priority";
const std::string purposeName = "purpose";

void storePriority(const std::string& flashId, uint8_t priority)
{
    std::error_code ec;
    auto path = fs::path(PERSIST_DIR) / flashId;
    if (!fs::is_directory(path, ec))
    {
        if (fs::exists(path, ec))
        {
            // Delete if it's a non-directory file
            warning("Removing non-directory file: {PATH}", "PATH", path);
            fs::remove_all(path, ec);
        }
        fs::create_directories(path, ec);
    }
    path = path / priorityName;

    std::ofstream os(path.c_str());
    cereal::JSONOutputArchive oarchive(os);
    oarchive(cereal::make_nvp(priorityName, priority));
}

void storePurpose(const std::string& flashId, VersionPurpose purpose)
{
    std::error_code ec;
    auto path = fs::path(PERSIST_DIR) / flashId;
    if (!fs::is_directory(path, ec))
    {
        if (fs::exists(path, ec))
        {
            // Delete if it's a non-directory file
            warning("Removing non-directory file: {PATH}", "PATH", path);
            fs::remove_all(path, ec);
        }
        fs::create_directories(path, ec);
    }
    path = path / purposeName;

    std::ofstream os(path.c_str());
    cereal::JSONOutputArchive oarchive(os);
    oarchive(cereal::make_nvp(purposeName, purpose));
}

bool restorePriority(const std::string& flashId, uint8_t& priority)
{
    std::error_code ec;
    auto path = fs::path(PERSIST_DIR) / flashId / priorityName;
    if (fs::exists(path, ec))
    {
        std::ifstream is(path.c_str(), std::ios::in);
        try
        {
            cereal::JSONInputArchive iarchive(is);
            iarchive(cereal::make_nvp(priorityName, priority));
            return true;
        }
        catch (const cereal::Exception& e)
        {
            fs::remove_all(path, ec);
        }
    }

    // Find the mtd device "u-boot-env" to retrieve the environment variables
    std::ifstream mtdDevices("/proc/mtd");
    std::string device;
    std::string devicePath;

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

            std::string versionVar = flashId + "=";
            auto varPosition = envVars.find(versionVar);

            if (varPosition != std::string::npos)
            {
                // Grab the environment variable for this flashId. These
                // variables follow the format "flashId=priority\0"
                auto var = envVars.substr(varPosition);
                priority = std::stoi(var.substr(versionVar.length()));
                return true;
            }
        }
    }
    catch (const std::exception& e)
    {
        error("Error during processing: {ERROR}", "ERROR", e);
    }

    return false;
}

bool restorePurpose(const std::string& flashId, VersionPurpose& purpose)
{
    std::error_code ec;
    auto path = fs::path(PERSIST_DIR) / flashId / purposeName;
    if (fs::exists(path, ec))
    {
        std::ifstream is(path.c_str(), std::ios::in);
        try
        {
            cereal::JSONInputArchive iarchive(is);
            iarchive(cereal::make_nvp(purposeName, purpose));
            return true;
        }
        catch (const cereal::Exception& e)
        {
            fs::remove_all(path, ec);
        }
    }

    return false;
}

void removePersistDataDirectory(const std::string& flashId)
{
    std::error_code ec;
    auto path = fs::path(PERSIST_DIR) / flashId;
    if (fs::exists(path, ec))
    {
        fs::remove_all(path, ec);
    }
}

} // namespace updater
} // namespace software
} // namespace phosphor
