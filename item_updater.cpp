#include <fstream>
#include <string>
#include <phosphor-logging/log.hpp>
#include "config.h"
#include "item_updater.hpp"
#include "xyz/openbmc_project/Software/Version/server.hpp"
#include <experimental/filesystem>
#include "version.hpp"

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
    auto purpose = server::Version::VersionPurpose::Unknown;
    std::string version;
    std::map<std::string,
             std::map<std::string,
                      sdbusplus::message::variant<std::string>>> interfaces;
    msg.read(objPath, interfaces);
    std::string path(std::move(objPath));

    for (const auto& intf : interfaces)
    {
        if (intf.first == VERSION_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Purpose")
                {
                    // Only process the BMC images
                    std::string str = sdbusplus::message::variant_ns::
                        get<std::string>(property.second);
                    auto value = server::Version::
                        convertVersionPurposeFromString(str);
                    if (value == server::Version::VersionPurpose::BMC ||
                        value == server::Version::VersionPurpose::System)
                    {
                        purpose = value;
                    }
                }
                else if (property.first == "Version")
                {
                    version = sdbusplus::message::variant_ns::
                        get<std::string>(property.second);
                }
            }
        }
    }
    if (purpose == server::Version::VersionPurpose::Unknown)
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
                     validateSquashFSImage(versionId);
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
        versions.insert(std::make_pair(
                            versionId,
                            std::make_unique<phosphor::software::
                                manager::Version>(
                                bus,
                                path,
                                version,
                                purpose,
                                "")));
    }
    return;
}

void ItemUpdater::processBMCImage()
{
    auto purpose = server::Version::VersionPurpose::BMC;
    auto version = phosphor::software::manager::Version::getBMCVersion();
    if (version.empty())
    {
        log<level::ERR>("Error reading current BMC version");
        return;
    }
    auto id = phosphor::software::manager::Version::getId(version);
    auto path =  std::string{SOFTWARE_OBJPATH} + '/' + id;
    activations.insert(std::make_pair(
                           id,
                           std::make_unique<Activation>(
                               bus,
                               path,
                               id,
                               server::Activation::Activations::Active)));
    versions.insert(std::make_pair(
                        id,
                        std::make_unique<phosphor::software::
                             manager::Version>(
                             bus,
                             path,
                             version,
                             purpose,
                             "")));
    return;
}

ItemUpdater::ActivationStatus ItemUpdater::validateSquashFSImage(
             const std::string& versionId)
{

    // TODO openbmc/openbmc#1715 Check the Common.FilePath to
    //      determine the active image.
    fs::path imageDirPath(IMG_UPLOAD_DIR);
    imageDirPath /= versionId;
    if (!fs::is_directory(imageDirPath))
    {
        return ItemUpdater::ActivationStatus::active;
    }

    fs::path file(imageDirPath);
    file /= bmcImage;
    std::ifstream efile(file.c_str());

    if (efile.good() == 1)
    {
        return ItemUpdater::ActivationStatus::ready;
    }
    else
    {
        log<level::ERR>("Failed to find the SquashFS image.");
        return ItemUpdater::ActivationStatus::invalid;
    }
}

} // namespace updater
} // namespace software
} // namespace phosphor
