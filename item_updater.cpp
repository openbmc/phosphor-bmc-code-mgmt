#include <fstream>
#include <string>
#include <phosphor-logging/log.hpp>
#include "config.h"
#include "item_updater.hpp"
#include "xyz/openbmc_project/Software/Version/server.hpp"
#include <experimental/filesystem>
#include "version.hpp"
#include "serialize.hpp"

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

    using SVersion = server::Version;
    using VersionPurpose = SVersion::VersionPurpose;
    namespace mesg = sdbusplus::message;
    namespace variant_ns = mesg::variant_ns;

    mesg::object_path objPath;
    auto purpose = VersionPurpose::Unknown;
    std::string version;
    std::map<std::string,
             std::map<std::string,
                      mesg::variant<std::string>>> interfaces;
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
                    auto value = SVersion::convertVersionPurposeFromString(
                        variant_ns::get<std::string>(property.second));
                    if (value == VersionPurpose::BMC ||
                        value == VersionPurpose::System)
                    {
                        purpose = value;
                    }
                }
                else if (property.first == "Version")
                {
                    version = variant_ns::get<std::string>(property.second);
                }
            }
        }
        else if (intf.first == FILEPATH_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Path")
                {
                    filePath = variant_ns::get<std::string>(property.second);
                }
            }
        }
    }
    if (version.empty() ||
        filePath.empty() ||
        purpose == VersionPurpose::Unknown)
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
                                        *this,
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
                                filePath,
                                std::bind(&ItemUpdater::erase,
                                          this,
                                          std::placeholders::_1))));
    }
    return;
}

void ItemUpdater::processBMCImage()
{
    // Read os-release from folders under /media/ to get
    // BMC Software Versions.
    for(const auto& iter : fs::directory_iterator(MEDIA_DIR))
    {
        auto activationState = server::Activation::Activations::Active;
        static const auto BMC_RO_PREFIX_LEN = strlen(BMC_RO_PREFIX);

        // Check if the BMC_RO_PREFIXis the prefix of the iter.path
        if (0 == iter.path().native().compare(0, BMC_RO_PREFIX_LEN,
                                              BMC_RO_PREFIX))
        {
            auto osRelease = iter.path() / OS_RELEASE_FILE;
            if (!fs::is_regular_file(osRelease))
            {
                log<level::ERR>("Failed to read osRelease\n",
                                entry("FileName=%s", osRelease.string()));
                activationState = server::Activation::Activations::Invalid;
            }
            auto version = 
                    phosphor::software::manager::Version::
                            getBMCVersion(osRelease);
            if (version.empty())
            {
                log<level::ERR>("Failed to read version from osRelease",
                                entry("FILENAME=%s", osRelease.string()));
                activationState = server::Activation::Activations::Invalid;
            }
            // The versionId is extracted from the path
            // for example /media/ro-2a1022fe
            auto id = iter.path().native().substr(BMC_RO_PREFIX_LEN);
            auto purpose = server::Version::VersionPurpose::BMC;
            auto path = fs::path(SOFTWARE_OBJPATH) / id;

            // Create Activation instance for this version.
            activations.insert(std::make_pair(
                                   id,
                                   std::make_unique<Activation>(
                                       bus,
                                       path,
                                       *this,
                                       id,
                                       activationState)));

            // If Active, create RedundancyPriority instance for this version.
            if (activationState == server::Activation::Activations::Active)
            {
                if(fs::is_regular_file(PERSIST_DIR + id))
                {
                    uint8_t priority;
                    restoreFromFile(id, &priority);
                    activations.find(id)->second->redundancyPriority =
                            std::make_unique<RedundancyPriority>(
                                 bus,
                                 path,
                                 *(activations.find(id)->second),
                                 priority);
                }
                else
                {
                    activations.find(id)->second->activation(
                            server::Activation::Activations::Invalid);
                }
            }

            // Create Version instance for this version.
            versions.insert(std::make_pair(
                                id,
                                std::make_unique<
                                     phosphor::software::manager::Version>(
                                     bus,
                                     path,
                                     version,
                                     purpose,
                                     "",
                                     std::bind(&ItemUpdater::erase,
                                       this,
                                       std::placeholders::_1)))); 

        }
    }
    return;
}

void ItemUpdater::erase(std::string entryId)
{
    // Delete ReadWrite and ReadOnly partitions
    removeReadWritePartition(entryId);
    removeReadOnlyPartition(entryId);

    // Removing entry in versions map
    auto it = versions.find(entryId);
    if (it == versions.end())
    {
        log<level::ERR>(("Error: Failed to find version " + entryId + \
                        " in item updater versions map." \
                        " Unable to remove.").c_str());
        return;
    }
    this->versions.erase(entryId);

    // Removing entry in activations map
    auto ita = activations.find(entryId);
    if (ita == activations.end())
    {
        log<level::ERR>(("Error: Failed to find version " + entryId + \
                        " in item updater activations map." \
                        " Unable to remove.").c_str());
        return;
    }
    // TODO: openbmc/openbmc#1986
    //       Test if this is the currently running image
    //       If not, don't continue.

    this->activations.erase(entryId);
    removeFile(entryId);
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

void ItemUpdater::freePriority(uint8_t value)
{
    //TODO openbmc/openbmc#1896 Improve the performance of this function
    for (const auto& intf : activations)
    {
        if(intf.second->redundancyPriority)
        {
            if (intf.second->redundancyPriority.get()->priority() == value)
            {
                intf.second->redundancyPriority.get()->priority(value+1);
            }
        }
    }
}

void ItemUpdater::reset()
{
    // Mark the read-write partition for recreation upon reboot.
    auto method = bus.new_method_call(
            SYSTEMD_BUSNAME,
            SYSTEMD_PATH,
            SYSTEMD_INTERFACE,
            "StartUnit");
    method.append("obmc-flash-bmc-setenv@rwreset=true.service", "replace");
    bus.call_noreply(method);

    log<level::INFO>("BMC factory reset will take effect upon reboot.");

    return;
}

void ItemUpdater::removeReadOnlyPartition(std::string versionId)
{
    auto serviceFile = "obmc-flash-bmc-ubiro-remove@" + versionId +
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
    auto serviceFile = "obmc-flash-bmc-ubirw-remove.service";

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
