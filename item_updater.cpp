#include <fstream>
#include <string>
#include <phosphor-logging/log.hpp>
#include "config.h"
#include "item_updater.hpp"
#include "xyz/openbmc_project/Software/Version/server.hpp"
#include <experimental/filesystem>

namespace phosphor
{
namespace software
{
namespace updater
{

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Software::server;

using namespace phosphor::logging;
namespace fs = std::experimental::filesystem;

constexpr auto bmcImage = "image-rofs";

void ItemUpdater::createActivation(sdbusplus::message::message& msg)
{
    sdbusplus::message::object_path objPath;
    std::map<std::string,
             std::map<std::string,
                      sdbusplus::message::variant<std::string>>> interfaces;
    msg.read(objPath, interfaces);
    std::string path(std::move(objPath));
    std::string filePath;

    for (const auto& intf : interfaces)
    {
        if (intf.first == VERSION_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Purpose")
                {
                    // Only process the BMC and System images
                    std::string value = sdbusplus::message::variant_ns::get<
                            std::string>(property.second);
                    if ((value != convertForMessage(
                            server::Version::VersionPurpose::BMC)) &&
                        (value != convertForMessage(
                            server::Version::VersionPurpose::System)))
                    {
                        return;
                    }
                }
            }
        }
        else if (intf.first == FILEPATH_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Path")
                {
                    filePath = sdbusplus::message::variant_ns::get<
                            std::string>(property.second);
                }
            }
        }
    }
    if (filePath.empty())
    {
        return;
    }

    // Version id is the last item in the path
    auto pos = path.rfind("/");
    if (pos == std::string::npos)
    {
        log<level::ERR>("No version id found in object path",
                        entry("OBJPATH=%s", path));
        return;
    }

    auto versionId = path.substr(pos + 1);

    if (activations.find(versionId) == activations.end())
    {
        // Determine the Activation state by processing the given image dir.
        auto activationState = server::Activation::Activations::Invalid;
        ItemUpdater::ActivationStatus result = ItemUpdater::
                     validateSquashFSImage(filePath);
        if (result == ItemUpdater::ActivationStatus::ready)
        {
            activationState = server::Activation::Activations::Ready;
        }
        else if (result == ItemUpdater::ActivationStatus::active)
        {
            activationState = server::Activation::Activations::Active;
        }
        activations.insert(std::make_pair(
                                        versionId,
                                        std::make_unique<Activation>(
                                            bus,
                                            path,
                                            versionId,
                                            activationState)));
    }
    return;
}

ItemUpdater::ActivationStatus ItemUpdater::validateSquashFSImage(
             const std::string& filePath)
{ 
    if (filePath == "Active")
    {
        return ItemUpdater::ActivationStatus::active;
    }

    fs::path file(filePath);
    file /= bmcImage;
    std::ifstream efile(file.c_str());

    if (efile.good() == 1)
    {
        return ItemUpdater::ActivationStatus::ready;
    }
    else
    {
        log<level::ERR>("Failed to find bmc image.");
        return ItemUpdater::ActivationStatus::invalid;
    }
}

} // namespace updater
} // namespace software
} // namespace phosphor
