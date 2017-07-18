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
    std::string filePath;

    for (const auto& intf : interfaces)
    {
        if (intf.first == VERSION_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Purpose")
                {
                    std::string str = sdbusplus::message::variant_ns::
                        get<std::string>(property.second);
                    purpose = server::Version::
                        convertVersionPurposeFromString(str);
                }
                else if (property.first == "Version")
                {
                    version = sdbusplus::message::variant_ns::
                        get<std::string>(property.second);
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
    if (version.empty() ||
        filePath.empty() ||
        (purpose != server::Version::VersionPurpose::BMC &&
        purpose != server::Version::VersionPurpose::System))
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
                                manager::Version<ItemUpdater>>(
                                bus,
                                path,
                                version,
                                purpose,
                                filePath,
                                this)));
    }
    return;
}

void ItemUpdater::processBMCImage()
{
    auto purpose = server::Version::VersionPurpose::BMC;
    auto version = phosphor::software::manager::Version<ItemUpdater>::
                   getBMCVersion();
    auto id = phosphor::software::manager::Version<ItemUpdater>::getId(version);
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
                             manager::Version<ItemUpdater>>(
                             bus,
                             path,
                             version,
                             purpose,
                             "",
                             this)));
    return;
}

void ItemUpdater::erase(std::string entryId)
{
    std::string found_key = "";

    for (const auto& itv : versions)
    {
        std::string current_key = itv.first;
        std::string current_version = (*(itv.second)).version();

        if (entryId.compare(current_version) == 0)
        {
            found_key = current_key;
            break;
        }
    }
    if (!found_key.empty())
    {
        std::string error_msg = "Error Failed to find version " + \
            found_key + " in item updater. Unable to delete.";
        log<level::ERR>(error_msg.c_str());
        return;
    }
    auto ita = activations.find(found_key);
    if (ita == activations.end())
    {
        std::string error_msg = "Error Failed to find activation " + \
            found_key + " in item updater. Unable to delete.";
        log<level::ERR>(error_msg.c_str());
        return;
    }

    // TODO: Test if this is the currently running image
    //       If not, don't continue.

    // Delete ReadWrite, ReadOnly partitions
    removeReadWritePartition(found_key);
    removeReadOnlyPartition(found_key);

    // Remove activation and version
    this->activations.erase(found_key);
    this->versions.erase(found_key);

}

ItemUpdater::ActivationStatus ItemUpdater::validateSquashFSImage(
             const std::string& filePath)
{

    fs::path file(filePath);
    file /= bmcImage;
    std::ifstream efile(file.c_str());

    if (efile.good() == 1)
    {
        return ItemUpdater::ActivationStatus::ready;
    }
    else
    {
        log<level::ERR>("Failed to find the BMC image.");
        return ItemUpdater::ActivationStatus::invalid;
    }
}

void ItemUpdater::removeReadOnlyPartition(std::string versionId)
{
        auto serviceFile = "obmc-flash-bios-ubiumount-ro@" + versionId +
                ".service";

        // Remove the read-only partitions.
        auto method = bus.new_method_call(
                SYSTEMD_BUSNAME,
                SYSTEMD_PATH,
                SYSTEMD_INTERFACE,
                "StartUnit");
        method.append(serviceFile, "replace");
        bus.call_noreply(method);
}

void ItemUpdater::removeReadWritePartition(std::string versionId)
{
        auto serviceFile = "obmc-flash-bios-ubiumount-rw@" + versionId +
                ".service";

        // Remove the read-write partitions.
        auto method = bus.new_method_call(
                SYSTEMD_BUSNAME,
                SYSTEMD_PATH,
                SYSTEMD_INTERFACE,
                "StartUnit");
        method.append(serviceFile, "replace");
        bus.call_noreply(method);
}

} // namespace updater
} // namespace software
} // namespace phosphor
