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
namespace control = sdbusplus::xyz::openbmc_project::Control::server;

using namespace phosphor::logging;
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
        if (result == ItemUpdater::ActivationStatus::ready)
        {
            activationState = server::Activation::Activations::Ready;
        }

        // Create an association to the BMC inventory item
        AssociationList associations{(std::make_tuple(
                                          ACTIVATION_FWD_ASSOCIATION,
                                          ACTIVATION_REV_ASSOCIATION,
                                          bmcInventoryPath))};

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

    auto purpose = server::Version::VersionPurpose::BMC;
    auto version = phosphor::software::manager::Version::getBMCVersion();
    auto id = phosphor::software::manager::Version::getId(version);
    auto path =  std::string{SOFTWARE_OBJPATH} + '/' + id;

    // Create an association to the BMC inventory item
    AssociationList associations{(std::make_tuple(
                                      ACTIVATION_FWD_ASSOCIATION,
                                      ACTIVATION_REV_ASSOCIATION,
                                      bmcInventoryPath))};

    activations.insert(std::make_pair(
                           id,
                           std::make_unique<Activation>(
                               bus,
                               path,
                               *this,
                               id,
                               server::Activation::Activations::Active,
                               associations)));
    versions.insert(std::make_pair(
                        id,
                        std::make_unique<VersionClass>(
                             bus,
                             path,
                             version,
                             purpose,
                             "")));

    return;
}

void ItemUpdater::erase(std::string entryId)
{
    // Delete ReadOnly partitions
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

void ItemUpdater::freePriority(uint8_t value)
{
    //TODO openbmc/openbmc#1896 Improve the performance of this function
    for (const auto& intf : activations)
    {
        if (intf.second->redundancyPriority)
        {
            if (intf.second->redundancyPriority.get()->priority() == value)
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

} // namespace updater
} // namespace software
} // namespace phosphor
