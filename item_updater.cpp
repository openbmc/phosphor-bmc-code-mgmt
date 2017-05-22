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

    for (const auto& intf : interfaces)
    {
        if (intf.first.compare(VERSION_IFACE))
        {
            continue;
        }

        for (const auto& property : intf.second)
        {
            if (!property.first.compare("Purpose"))
            {
                // Only process the BMC images
                auto value = sdbusplus::message::variant_ns::get<std::string>(
                        property.second);
                if (value !=
                    convertForMessage(server::Version::VersionPurpose::BMC) &&
                    value !=
                    convertForMessage(server::Version::VersionPurpose::System))
                {
                    return;
                }
            }
        }
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
        auto result = ItemUpdater::validateSquashFSImage(versionId);
        if (result == 0)
        {
            activationState = server::Activation::Activations::Ready;
        }
        else if (result == 1)
        {
            activationState = server::Activation::Activations::Active;
        }
        updater->activations.insert(std::make_pair(
                                        versionId,
                                        std::make_unique<Activation>(
                                            updater->bus,
                                            path,
                                            versionId,
                                            activationState)));
    }
    return;
}

int ItemUpdater::validateSquashFSImage(const std::string& versionId)
{

    // TODO: Remove the validation for active image when
    //       active image is implemented in the MANAGER class
    fs::path imageDirPath(IMG_UPLOAD_DIR);
    imageDirPath /= versionId;
    if (!fs::is_directory(imageDirPath))
    {
        return 1;
    }

    fs::path file(imageDirPath);
    file /= bmcImage;
    std::ifstream efile(file.c_str());

    if (efile.good() == 1)
    {
        return 0;
    }
    else
    {
        log<level::ERR>("Failed to find the SquashFS image.");
        return -1;
    }
}

} // namespace updater
} // namespace software
} // namespace phosphor
