#include <fstream>
#include <string>
#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog.hpp>
#include <elog-errors.hpp>
#include <xyz/openbmc_project/Software/Version/error.hpp>
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
namespace control = sdbusplus::xyz::openbmc_project::Control::server;

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Software::Version::Error;
namespace fs = std::experimental::filesystem;

const std::vector<std::string> bmcImages = { "image-kernel",
                                             "image-rofs",
                                             "image-rwfs",
                                             "image-u-boot" };

void ItemUpdater::createActivation(sdbusplus::message::message& msg)
{

    using SVersion = server::Version;
    using VersionPurpose = SVersion::VersionPurpose;
    using VersionClass = phosphor::software::manager::Version;
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
        ItemUpdater::ActivationStatus result =
                ItemUpdater::validateSquashFSImage(filePath);
        AssociationList associations = {};

        if (result == ItemUpdater::ActivationStatus::ready)
        {
            activationState = server::Activation::Activations::Ready;
            // Create an association to the BMC inventory item
            associations.emplace_back(std::make_tuple(
                                              ACTIVATION_FWD_ASSOCIATION,
                                              ACTIVATION_REV_ASSOCIATION,
                                              bmcInventoryPath));
        }

        activations.insert(std::make_pair(
                               versionId,
                               std::make_unique<Activation>(
                                        bus,
                                        path,
                                        *this,
                                        versionId,
                                        activationState,
                                        associations)));
        versions.insert(std::make_pair(
                            versionId,
                            std::make_unique<VersionClass>(
                                bus,
                                path,
                                version,
                                purpose,
                                filePath)));
    }
    else
    {
        log<level::INFO>("Software Object with the same version already exists",
                         entry("VERSION_ID=%s", versionId));
    }
    return;
}

void ItemUpdater::processBMCImage()
{
    using VersionClass = phosphor::software::manager::Version;
    // Read os-release from /etc/ to get the functional BMC version
    auto functionalVersion = VersionClass::getBMCVersion(OS_RELEASE_FILE);

    // Read os-release from folders under /media/ to get
    // BMC Software Versions.
    for(const auto& iter : fs::directory_iterator(MEDIA_DIR))
    {
        auto activationState = server::Activation::Activations::Active;
        static const auto BMC_RO_PREFIX_LEN = strlen(BMC_ROFS_PREFIX);

        // Check if the BMC_RO_PREFIXis the prefix of the iter.path
        if (0 == iter.path().native().compare(0, BMC_RO_PREFIX_LEN,
                                              BMC_ROFS_PREFIX))
        {
            auto osRelease = iter.path() / OS_RELEASE_FILE;
            if (!fs::is_regular_file(osRelease))
            {
                log<level::ERR>("Failed to read osRelease\n",
                                entry("FileName=%s", osRelease.string()));
                activationState = server::Activation::Activations::Invalid;
            }
            auto version = VersionClass::getBMCVersion(osRelease);
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

            // Create functional association if this is the functional version
            if (version.compare(functionalVersion) == 0)
            {
                createFunctionalAssociation(path);
            }

            AssociationList associations = {};

            if (activationState == server::Activation::Activations::Active)
            {
                // Create an association to the BMC inventory item
                associations.emplace_back(std::make_tuple(
                                                  ACTIVATION_FWD_ASSOCIATION,
                                                  ACTIVATION_REV_ASSOCIATION,
                                                  bmcInventoryPath));

                // Create an active association since this image is active
                createActiveAssociation(path);
            }

            // Create Activation instance for this version.
            activations.insert(std::make_pair(
                                   id,
                                   std::make_unique<Activation>(
                                       bus,
                                       path,
                                       *this,
                                       id,
                                       server::Activation::Activations::Active,
                                       associations)));

            // If Active, create RedundancyPriority instance for this version.
            if (activationState == server::Activation::Activations::Active)
            {
                uint8_t priority = std::numeric_limits<uint8_t>::max();
                if (!restoreFromFile(id, priority))
                {
                    log<level::ERR>("Unable to restore priority from file.",
                            entry("VERSIONID=%s", id));
                }
                activations.find(id)->second->redundancyPriority =
                        std::make_unique<RedundancyPriority>(
                             bus,
                             path,
                             *(activations.find(id)->second),
                             priority);
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
                                     "")));
        }
    }

    // If there is no ubi volume for bmc version then read the /etc/os-release
    // and create rofs-<versionId> under /media
    if (activations.size() == 0)
    {
        auto version =
                phosphor::software::manager::Version::
                        getBMCVersion(OS_RELEASE_FILE);
        auto id = phosphor::software::manager::Version::getId(version);
        auto versionFileDir = BMC_ROFS_PREFIX + id + "/etc/";
        try
        {
            if(!fs::is_directory(versionFileDir))
            {
                fs::create_directories(versionFileDir);
            }
            auto versionFilePath = BMC_ROFS_PREFIX + id + OS_RELEASE_FILE;
            fs::create_directory_symlink(OS_RELEASE_FILE, versionFilePath);
            ItemUpdater::processBMCImage();
        }
        catch (const filesystem_error& e)
        {
            log<level::ERR>(e.what());
        }
    }
    return;
}

void ItemUpdater::erase(std::string entryId)
{
    // Find entry in versions map
    auto it = versions.find(entryId);
    if (it != versions.end())
    {
        if (it->second->isFunctional())
        {
            log<level::ERR>(("Error: Version " + entryId + \
                             " is currently running on the BMC." \
                             " Unable to remove.").c_str());
             return;
        }

        // Delete ReadOnly partitions if it's not active
        removeReadOnlyPartition(entryId);
        removeFile(entryId);
    }
    else
    {
        // Delete ReadOnly partitions even if we can't find the version
        removeReadOnlyPartition(entryId);
        removeFile(entryId);

        log<level::ERR>(("Error: Failed to find version " + entryId + \
                         " in item updater versions map." \
                         " Unable to remove.").c_str());
        return;
    }

    // Remove the priority environment variable.
    auto serviceFile = "obmc-flash-bmc-setenv@" + entryId + ".service";
    auto method = bus.new_method_call(
            SYSTEMD_BUSNAME,
            SYSTEMD_PATH,
            SYSTEMD_INTERFACE,
            "StartUnit");
    method.append(serviceFile, "replace");
    bus.call_noreply(method);

    // Removing entry in versions map
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

    this->activations.erase(entryId);
}

void ItemUpdater::deleteAll()
{
    std::vector<std::string> deletableVersions;

    for (const auto& versionIt : versions)
    {
        if (!versionIt.second->isFunctional())
        {
            deletableVersions.push_back(versionIt.first);
        }
    }

    for (const auto& deletableIt : deletableVersions)
    {
        ItemUpdater::erase(deletableIt);
    }

    // Remove any volumes that do not match current versions.
    auto method = bus.new_method_call(
            SYSTEMD_BUSNAME,
            SYSTEMD_PATH,
            SYSTEMD_INTERFACE,
            "StartUnit");
    method.append("obmc-flash-bmc-cleanup.service", "replace");
    bus.call_noreply(method);
}

ItemUpdater::ActivationStatus ItemUpdater::validateSquashFSImage(
        const std::string& filePath)
{
    bool invalid = false;

    for (auto& bmcImage : bmcImages)
    {
        fs::path file(filePath);
        file /= bmcImage;
        std::ifstream efile(file.c_str());
        if (efile.good() != 1)
        {
            log<level::ERR>("Failed to find the BMC image.",
                            entry("IMAGE=%s", bmcImage.c_str()));
            invalid = true;
        }
    }

    if (invalid)
    {
        return ItemUpdater::ActivationStatus::invalid;
    }

    return ItemUpdater::ActivationStatus::ready;
}

void ItemUpdater::freePriority(uint8_t value, const std::string& versionId)
{
    //TODO openbmc/openbmc#1896 Improve the performance of this function
    for (const auto& intf : activations)
    {
        if (intf.second->redundancyPriority)
        {
            if (intf.second->redundancyPriority.get()->priority() == value &&
                intf.second->versionId != versionId)
            {
                intf.second->redundancyPriority.get()->priority(value + 1);
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
    method.append("obmc-flash-bmc-setenv@rwreset\\x3dtrue.service", "replace");
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

bool ItemUpdater::fieldModeEnabled(bool value)
{
    // enabling field mode is intended to be one way: false -> true
    if (value && !control::FieldMode::fieldModeEnabled())
    {
        control::FieldMode::fieldModeEnabled(value);

        auto method = bus.new_method_call(
                SYSTEMD_BUSNAME,
                SYSTEMD_PATH,
                SYSTEMD_INTERFACE,
                "StartUnit");
        method.append("obmc-flash-bmc-setenv@fieldmode\\x3dtrue.service",
                      "replace");
        bus.call_noreply(method);

        method = bus.new_method_call(
                SYSTEMD_BUSNAME,
                SYSTEMD_PATH,
                SYSTEMD_INTERFACE,
                "StopUnit");
        method.append("usr-local.mount", "replace");
        bus.call_noreply(method);

        std::vector<std::string> usrLocal = {"usr-local.mount"};

        method = bus.new_method_call(
                SYSTEMD_BUSNAME,
                SYSTEMD_PATH,
                SYSTEMD_INTERFACE,
                "MaskUnitFiles");
        method.append(usrLocal, false, true);
        bus.call_noreply(method);
    }

    return control::FieldMode::fieldModeEnabled();
}

void ItemUpdater::restoreFieldModeStatus()
{
    std::ifstream input("/run/fw_env");
    std::string envVar;
    std::getline(input, envVar);

    if (envVar.find("fieldmode=true") != std::string::npos)
    {
        ItemUpdater::fieldModeEnabled(true);
    }
}

void ItemUpdater::setBMCInventoryPath()
{
    //TODO: openbmc/openbmc#1786 - Get the BMC path by looking for objects
    //      that implement the BMC inventory interface
    auto depth = 0;
    auto mapperCall = bus.new_method_call(MAPPER_BUSNAME,
                                          MAPPER_PATH,
                                          MAPPER_INTERFACE,
                                          "GetSubTreePaths");

    mapperCall.append(CHASSIS_INVENTORY_PATH);
    mapperCall.append(depth);

    // TODO: openbmc/openbmc#2226 - Add Inventory Item filter when
    //       mapper is fixed.
    std::vector<std::string> filter = {};
    mapperCall.append(filter);

    auto response = bus.call(mapperCall);
    if (response.is_method_error())
    {
        log<level::ERR>("Error in mapper GetSubTreePath");
        return;
    }

    using ObjectPaths = std::vector<std::string>;
    ObjectPaths result;
    response.read(result);

    if (result.empty())
    {
        log<level::ERR>("Invalid response from mapper");
        return;
    }

    for (auto& iter : result)
    {
        const auto& path = iter;
        if (path.substr(path.find_last_of('/') + 1).compare("bmc") == 0)
        {
            bmcInventoryPath = path;
            return;
        }
    }
}

void ItemUpdater::createActiveAssociation(const std::string& path)
{
    assocs.emplace_back(std::make_tuple(ACTIVE_FWD_ASSOCIATION,
                                        ACTIVE_REV_ASSOCIATION,
                                        path));
    associations(assocs);
}

void ItemUpdater::createFunctionalAssociation(const std::string& path)
{
    assocs.emplace_back(std::make_tuple(FUNCTIONAL_FWD_ASSOCIATION,
                                        FUNCTIONAL_REV_ASSOCIATION,
                                        path));
    associations(assocs);
}

void ItemUpdater::removeActiveAssociation(const std::string& path)
{
    for (auto iter = assocs.begin(); iter != assocs.end();)
    {
        // Since there could be multiple associations to the same path,
        // only remove ones that have an active forward association.
        if ((std::get<0>(*iter)).compare(ACTIVE_FWD_ASSOCIATION) == 0 &&
            (std::get<2>(*iter)).compare(path) == 0)
        {
            iter = assocs.erase(iter);
            associations(assocs);
        }
        else
        {
            ++iter;
        }
    }
}

bool ItemUpdater::isLowestPriority(uint8_t value)
{
    for (const auto& intf : activations)
    {
        if(intf.second->redundancyPriority)
        {
            if (intf.second->redundancyPriority.get()->priority() < value)
            {
                return false;
            }
        }
    }
    return true;
}

} // namespace updater
} // namespace software
} // namespace phosphor
