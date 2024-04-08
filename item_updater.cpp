#include "config.h"

#include "item_updater.hpp"

#include "images.hpp"
#include "serialize.hpp"
#include "version.hpp"
#include "xyz/openbmc_project/Software/ExtendedVersion/server.hpp"
#include "xyz/openbmc_project/Software/Version/server.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Software/Image/error.hpp>

#include <filesystem>
#include <fstream>
#include <queue>
#include <set>
#include <string>
#include <system_error>

namespace phosphor
{
namespace software
{
namespace updater
{

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::server::xyz::openbmc_project::software;
namespace control = sdbusplus::server::xyz::openbmc_project::control;

PHOSPHOR_LOG2_USING;
using namespace phosphor::logging;
using namespace sdbusplus::error::xyz::openbmc_project::software::image;
using namespace phosphor::software::image;
namespace fs = std::filesystem;
using NotAllowed = sdbusplus::error::xyz::openbmc_project::common::NotAllowed;

void ItemUpdater::createActivation(sdbusplus::message_t& msg)
{
    using SVersion = server::Version;
    using VersionPurpose = SVersion::VersionPurpose;

    sdbusplus::message::object_path objPath;
    auto purpose = VersionPurpose::Unknown;
    std::string extendedVersion;
    std::string version;
    std::map<std::string,
             std::map<std::string,
                      std::variant<std::string, std::vector<std::string>>>>
        interfaces;
    msg.read(objPath, interfaces);
    std::string path(std::move(objPath));
    std::string filePath;
    std::vector<std::string> compatibleNames;

    for (const auto& intf : interfaces)
    {
        if (intf.first == VERSION_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Purpose")
                {
                    auto value = SVersion::convertVersionPurposeFromString(
                        std::get<std::string>(property.second));
                    if (value == VersionPurpose::BMC ||
#ifdef HOST_BIOS_UPGRADE
                        value == VersionPurpose::Host ||
#endif
                        value == VersionPurpose::System)
                    {
                        purpose = value;
                    }
                }
                else if (property.first == "Version")
                {
                    version = std::get<std::string>(property.second);
                }
            }
        }
        else if (intf.first == FILEPATH_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Path")
                {
                    filePath = std::get<std::string>(property.second);
                }
            }
        }
        else if (intf.first == EXTENDED_VERSION_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "ExtendedVersion")
                {
                    extendedVersion = std::get<std::string>(property.second);
                }
            }
        }
        else if (intf.first == COMPATIBLE_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Names")
                {
                    compatibleNames =
                        std::get<std::vector<std::string>>(property.second);
                }
            }
        }
    }
    if (version.empty() || filePath.empty() ||
        purpose == VersionPurpose::Unknown)
    {
        return;
    }

    // Version id is the last item in the path
    auto pos = path.rfind("/");
    if (pos == std::string::npos)
    {
        error("No version id found in object path: {PATH}", "PATH", path);
        return;
    }

    auto versionId = path.substr(pos + 1);

    if (activations.find(versionId) == activations.end())
    {
        verifyAndCreateObjects(versionId, path, version, purpose,
                               extendedVersion, filePath, compatibleNames);
    }
    return;
}

void ItemUpdater::createActivationWithApplyTime(
    std::string& id, std::string& path,
    ApplyTimeIntf::RequestedApplyTimes applyTime)
{
    info("Creating Activation object for id: {ID}", "ID", id);
    AssociationList associations = {};
    // Create an association to the BMC inventory item
    associations.emplace_back(std::make_tuple(ACTIVATION_FWD_ASSOCIATION,
                                              ACTIVATION_REV_ASSOCIATION,
                                              bmcInventoryPath));
    activations.insert(std::make_pair(
        id, std::make_unique<Activation>(
                bus, path, *this, id, server::Activation::Activations::NotReady,
                associations)));
    activations[id]->applyTime =
        std::make_unique<ApplyTimeIntf::RequestedApplyTimes>(applyTime);
}

ActivationIntf::Activations ItemUpdater::verifyAndCreateObjects(
    std::string& id, std::string& path, std::string& version,
    VersionClass::VersionPurpose purpose, std::string& extendedVersion,
    std ::string& filePath, std::vector<std::string>& compatibleNames)
{
    // Determine the Activation state by processing the given image dir.
    auto activationState = server::Activation::Activations::Invalid;
    ItemUpdater::ActivationStatus result;
    if (purpose == VersionPurpose::BMC || purpose == VersionPurpose::System)
    {
        result = ItemUpdater::validateSquashFSImage(filePath);
    }
    else
    {
        result = ItemUpdater::ActivationStatus::ready;
    }

    AssociationList associations = {};

    if (result == ItemUpdater::ActivationStatus::ready)
    {
        activationState = server::Activation::Activations::Ready;
        // Create an association to the BMC inventory item
        associations.emplace_back(std::make_tuple(ACTIVATION_FWD_ASSOCIATION,
                                                  ACTIVATION_REV_ASSOCIATION,
                                                  bmcInventoryPath));
    }

    auto versionPtr = std::make_unique<VersionClass>(
        bus, path, version, purpose, extendedVersion, filePath, compatibleNames,
        std::bind(&ItemUpdater::erase, this, std::placeholders::_1), id);
    versionPtr->deleteObject =
        std::make_unique<phosphor::software::manager::Delete>(bus, path,
                                                              *versionPtr);
    versions.insert(std::make_pair(id, std::move(versionPtr)));

    auto activation = activations.find(id);
    if (activation == activations.end())
    {
        activations.insert(std::make_pair(
            id, std::make_unique<Activation>(bus, path, *this, id,
                                             activationState, associations)));
    }
    else
    {
        activation->second->activation(activationState);
    }
    return activationState;
}

bool ItemUpdater::requestActivation(std::string& id)
{
    auto activation = activations.find(id);
    if (activation == activations.end())
    {
        error("Activation object not found for id: {ID}", "ID", id);
        return false;
    }
    activation->second->requestedActivation(
        server::Activation::RequestedActivations::Active);
    return true;
}

bool ItemUpdater::updateActivationStatus(std::string& id,
                                         ActivationIntf::Activations status)
{
    auto activation = activations.find(id);
    if (activation == activations.end())
    {
        error("Activation object not found for id: {ID}", "ID", id);
        return false;
    }
    activation->second->activation(status);
    return true;
}

void ItemUpdater::processBMCImage()
{
    using VersionClass = phosphor::software::manager::Version;

    // Check MEDIA_DIR and create if it does not exist
    try
    {
        if (!fs::is_directory(MEDIA_DIR))
        {
            fs::create_directory(MEDIA_DIR);
        }
    }
    catch (const fs::filesystem_error& e)
    {
        error("Failed to prepare dir: {ERROR}", "ERROR", e);
        return;
    }

    // Functional images are mounted as rofs-<location>-functional
    constexpr auto functionalSuffix = "-functional";
    bool functionalFound = false;

    // Read os-release from folders under /media/ to get
    // BMC Software Versions.
    std::error_code ec;
    for (const auto& iter : fs::directory_iterator(MEDIA_DIR, ec))
    {
        auto activationState = server::Activation::Activations::Active;
        static const auto BMC_RO_PREFIX_LEN = strlen(BMC_ROFS_PREFIX);

        // Check if the BMC_RO_PREFIXis the prefix of the iter.path
        if (0 ==
            iter.path().native().compare(0, BMC_RO_PREFIX_LEN, BMC_ROFS_PREFIX))
        {
            // Get the version to calculate the id
            fs::path releaseFile(OS_RELEASE_FILE);
            auto osRelease = iter.path() / releaseFile.relative_path();
            if (!fs::is_regular_file(osRelease, ec))
            {
#ifdef BMC_STATIC_DUAL_IMAGE
                // For dual image, it is possible that the secondary image is
                // empty or contains invalid data, ignore such case.
                info("Unable to find osRelease: {PATH}: {ERROR_MSG}", "PATH",
                     osRelease, "ERROR_MSG", ec.message());
#else
                error("Failed to read osRelease: {PATH}: {ERROR_MSG}", "PATH",
                      osRelease, "ERROR_MSG", ec.message());

                // Try to get the version id from the mount directory name and
                // call to delete it as this version may be corrupted. Dynamic
                // volumes created by the UBI layout for example have the id in
                // the mount directory name. The worst that can happen is that
                // erase() is called with an non-existent id and returns.
                auto id = iter.path().native().substr(BMC_RO_PREFIX_LEN);
                ItemUpdater::erase(id);
#endif

                continue;
            }
            auto version = VersionClass::getBMCVersion(osRelease);
            if (version.empty())
            {
                error("Failed to read version from osRelease: {PATH}", "PATH",
                      osRelease);

                // Try to delete the version, same as above if the
                // OS_RELEASE_FILE does not exist.
                auto id = iter.path().native().substr(BMC_RO_PREFIX_LEN);
                ItemUpdater::erase(id);

                continue;
            }

            // The flash location is part of the mount name: rofs-<location>
            auto flashId = iter.path().native().substr(BMC_RO_PREFIX_LEN);

            auto id = VersionClass::getId(version + flashId);

            // Check if the id has already been added. This can happen if the
            // BMC partitions / devices were manually flashed with the same
            // image.
            if (versions.find(id) != versions.end())
            {
                continue;
            }

            auto functional = false;
            if (iter.path().native().find(functionalSuffix) !=
                std::string::npos)
            {
                // Set functional to true and remove the functional suffix
                functional = true;
                flashId.erase(flashId.length() - strlen(functionalSuffix));
                functionalFound = true;
            }

            auto purpose = server::Version::VersionPurpose::BMC;
            restorePurpose(flashId, purpose);

            // Read os-release from /etc/ to get the BMC extended version
            std::string extendedVersion =
                VersionClass::getBMCExtendedVersion(osRelease);

            auto path = fs::path(SOFTWARE_OBJPATH) / id;

            // Create functional association if this is the functional
            // version
            if (functional)
            {
                createFunctionalAssociation(path);
            }

            AssociationList associations = {};

            if (activationState == server::Activation::Activations::Active)
            {
                // Create an association to the BMC inventory item
                associations.emplace_back(std::make_tuple(
                    ACTIVATION_FWD_ASSOCIATION, ACTIVATION_REV_ASSOCIATION,
                    bmcInventoryPath));

                // Create an active association since this image is active
                createActiveAssociation(path);
            }

            // All updateable firmware components must expose the updateable
            // association.
            createUpdateableAssociation(path);

            // Create Version instance for this version.
            auto versionPtr = std::make_unique<VersionClass>(
                bus, path, version, purpose, extendedVersion, flashId,
                std::vector<std::string>(),
                std::bind(&ItemUpdater::erase, this, std::placeholders::_1),
                id);
            if (functional)
            {
                versionPtr->setFunctional(true);
            }
            else
            {
                versionPtr->deleteObject =
                    std::make_unique<phosphor::software::manager::Delete>(
                        bus, path, *versionPtr);
            }
            versions.insert(std::make_pair(id, std::move(versionPtr)));

            // Create Activation instance for this version.
            activations.insert(std::make_pair(
                id, std::make_unique<Activation>(
                        bus, path, *this, id, activationState, associations)));

#ifdef BMC_STATIC_DUAL_IMAGE
            uint8_t priority;
            if ((functional && (runningImageSlot == 0)) ||
                (!functional && (runningImageSlot == 1)))
            {
                priority = 0;
            }
            else
            {
                priority = 1;
            }
            activations.find(id)->second->redundancyPriority =
                std::make_unique<RedundancyPriority>(
                    bus, path, *(activations.find(id)->second), priority,
                    false);
#else
            // If Active, create RedundancyPriority instance for this
            // version.
            if (activationState == server::Activation::Activations::Active)
            {
                uint8_t priority = std::numeric_limits<uint8_t>::max();
                if (!restorePriority(flashId, priority))
                {
                    if (functional)
                    {
                        priority = 0;
                    }
                    else
                    {
                        error(
                            "Unable to restore priority from file for {VERSIONID}",
                            "VERSIONID", id);
                    }
                }
                activations.find(id)->second->redundancyPriority =
                    std::make_unique<RedundancyPriority>(
                        bus, path, *(activations.find(id)->second), priority,
                        false);
            }
#endif
        }
    }

    if (!functionalFound)
    {
        // If there is no functional version found, read the /etc/os-release and
        // create rofs-<versionId>-functional under MEDIA_DIR, then call again
        // processBMCImage() to create the D-Bus interface for it.
        auto version = VersionClass::getBMCVersion(OS_RELEASE_FILE);
        auto id = phosphor::software::manager::Version::getId(version +
                                                              functionalSuffix);
        auto versionFileDir = BMC_ROFS_PREFIX + id + functionalSuffix + "/etc/";
        try
        {
            if (!fs::is_directory(versionFileDir))
            {
                fs::create_directories(versionFileDir);
            }
            auto versionFilePath = BMC_ROFS_PREFIX + id + functionalSuffix +
                                   OS_RELEASE_FILE;
            fs::create_directory_symlink(OS_RELEASE_FILE, versionFilePath);
            ItemUpdater::processBMCImage();
        }
        catch (const std::exception& e)
        {
            error("Exception during processing: {ERROR}", "ERROR", e);
        }
    }

    mirrorUbootToAlt();
    return;
}

void ItemUpdater::erase(std::string entryId)
{
    // Find entry in versions map
    auto it = versions.find(entryId);
    if (it != versions.end())
    {
        if (it->second->isFunctional() && ACTIVE_BMC_MAX_ALLOWED > 1)
        {
            error(
                "Version ({VERSIONID}) is currently running on the BMC; unable to remove.",
                "VERSIONID", entryId);
            return;
        }
    }

    // First call resetUbootEnvVars() so that the BMC points to a valid image to
    // boot from. If resetUbootEnvVars() is called after the image is actually
    // deleted from the BMC flash, there'd be a time window where the BMC would
    // be pointing to a non-existent image to boot from.
    // Need to remove the entries from the activations map before that call so
    // that resetUbootEnvVars() doesn't use the version to be deleted.
    auto iteratorActivations = activations.find(entryId);
    if (iteratorActivations == activations.end())
    {
        error(
            "Failed to find version ({VERSIONID}) in item updater activations map; unable to remove.",
            "VERSIONID", entryId);
    }
    else
    {
        removeAssociations(iteratorActivations->second->path);
        iteratorActivations->second->deleteImageManagerObject();
        this->activations.erase(entryId);
    }
    ItemUpdater::resetUbootEnvVars();

    if (it != versions.end())
    {
        auto flashId = it->second->path();

        // Delete version data if it has been installed on flash (path is not
        // the upload directory)
        if (flashId.find(IMG_UPLOAD_DIR) == std::string::npos)
        {
            removeReadOnlyPartition(entryId);
            removePersistDataDirectory(flashId);
            helper.clearEntry(flashId);
        }

        // Removing entry in versions map
        this->versions.erase(entryId);
    }

    return;
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

    helper.cleanup();
}

ItemUpdater::ActivationStatus
    ItemUpdater::validateSquashFSImage(const std::string& filePath)
{
    bool valid = true;

    // Record the images which are being updated
    // First check for the fullimage, then check for images with partitions
    imageUpdateList.push_back(bmcFullImages);
    valid = checkImage(filePath, imageUpdateList);
    if (!valid)
    {
        imageUpdateList.clear();
        imageUpdateList.assign(bmcImages.begin(), bmcImages.end());
        valid = checkImage(filePath, imageUpdateList);
        if (!valid)
        {
            error("Failed to find the needed BMC images.");
            return ItemUpdater::ActivationStatus::invalid;
        }
    }

    return ItemUpdater::ActivationStatus::ready;
}

void ItemUpdater::savePriority(const std::string& versionId, uint8_t value)
{
    auto flashId = versions.find(versionId)->second->path();
    storePriority(flashId, value);
    helper.setEntry(flashId, value);
}

void ItemUpdater::freePriority(uint8_t value, const std::string& versionId)
{
    std::map<std::string, uint8_t> priorityMap;

    // Insert the requested version and priority, it may not exist yet.
    priorityMap.insert(std::make_pair(versionId, value));

    for (const auto& intf : activations)
    {
        if (intf.second->redundancyPriority)
        {
            priorityMap.insert(std::make_pair(
                intf.first, intf.second->redundancyPriority.get()->priority()));
        }
    }

    // Lambda function to compare 2 priority values, use <= to allow duplicates
    typedef std::function<bool(std::pair<std::string, uint8_t>,
                               std::pair<std::string, uint8_t>)>
        cmpPriority;
    cmpPriority cmpPriorityFunc =
        [](std::pair<std::string, uint8_t> priority1,
           std::pair<std::string, uint8_t> priority2) {
        return priority1.second <= priority2.second;
    };

    // Sort versions by ascending priority
    std::set<std::pair<std::string, uint8_t>, cmpPriority> prioritySet(
        priorityMap.begin(), priorityMap.end(), cmpPriorityFunc);

    auto freePriorityValue = value;
    for (auto& element : prioritySet)
    {
        if (element.first == versionId)
        {
            continue;
        }
        if (element.second == freePriorityValue)
        {
            ++freePriorityValue;
            auto it = activations.find(element.first);
            it->second->redundancyPriority.get()->sdbusPriority(
                freePriorityValue);
        }
    }

    auto lowestVersion = prioritySet.begin()->first;
    if (value == prioritySet.begin()->second)
    {
        lowestVersion = versionId;
    }
    updateUbootEnvVars(lowestVersion);
}

void ItemUpdater::reset()
{
    helper.factoryReset();

    info("BMC factory reset will take effect upon reboot.");
}

void ItemUpdater::removeReadOnlyPartition(std::string versionId)
{
    auto flashId = versions.find(versionId)->second->path();
    helper.removeVersion(flashId);
}

bool ItemUpdater::fieldModeEnabled(bool value)
{
    // enabling field mode is intended to be one way: false -> true
    if (value && !control::FieldMode::fieldModeEnabled())
    {
        control::FieldMode::fieldModeEnabled(value);

        auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                          SYSTEMD_INTERFACE, "StartUnit");
        method.append("obmc-flash-bmc-setenv@fieldmode\\x3dtrue.service",
                      "replace");
        bus.call_noreply(method);

        method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                     SYSTEMD_INTERFACE, "StopUnit");
        method.append("usr-local.mount", "replace");
        bus.call_noreply(method);

        std::vector<std::string> usrLocal = {"usr-local.mount"};

        method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                     SYSTEMD_INTERFACE, "MaskUnitFiles");
        method.append(usrLocal, false, true);
        bus.call_noreply(method);
    }
    else if (!value && control::FieldMode::fieldModeEnabled())
    {
        elog<NotAllowed>(xyz::openbmc_project::common::NotAllowed::REASON(
            "FieldMode is not allowed to be cleared"));
    }

    return control::FieldMode::fieldModeEnabled();
}

void ItemUpdater::restoreFieldModeStatus()
{
    // The fieldmode u-boot environment variable may not exist since it is not
    // part of the default environment, run fw_printenv with 2>&1 to ignore the
    // error message in the journal "Error: "fieldmode" not defined"
    std::pair<int, std::string> ret = utils::execute("/sbin/fw_printenv", "-n",
                                                     "fieldmode", "2>&1");

    if (ret.first != 0)
    {
        return;
    }

    // truncate any extra characters off the end to compare against a "true" str
    std::string result = ret.second.substr(0, 4);
    if (result == "true")
    {
        ItemUpdater::fieldModeEnabled(true);
    }
}

void ItemUpdater::setBMCInventoryPath()
{
    auto depth = 0;
    auto mapperCall = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                          MAPPER_INTERFACE, "GetSubTreePaths");

    mapperCall.append(INVENTORY_PATH);
    mapperCall.append(depth);
    std::vector<std::string> filter = {BMC_INVENTORY_INTERFACE};
    mapperCall.append(filter);

    try
    {
        auto response = bus.call(mapperCall);

        using ObjectPaths = std::vector<std::string>;
        ObjectPaths result;
        response.read(result);

        if (!result.empty())
        {
            bmcInventoryPath = result.front();
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Error in mapper GetSubTreePath: {ERROR}", "ERROR", e);
        return;
    }

    return;
}

void ItemUpdater::createActiveAssociation(const std::string& path)
{
    assocs.emplace_back(
        std::make_tuple(ACTIVE_FWD_ASSOCIATION, ACTIVE_REV_ASSOCIATION, path));
    associations(assocs);
}

void ItemUpdater::createFunctionalAssociation(const std::string& path)
{
    assocs.emplace_back(std::make_tuple(FUNCTIONAL_FWD_ASSOCIATION,
                                        FUNCTIONAL_REV_ASSOCIATION, path));
    associations(assocs);
}

void ItemUpdater::createUpdateableAssociation(const std::string& path)
{
    assocs.emplace_back(std::make_tuple(UPDATEABLE_FWD_ASSOCIATION,
                                        UPDATEABLE_REV_ASSOCIATION, path));
    associations(assocs);
}

void ItemUpdater::removeAssociations(const std::string& path)
{
    for (auto iter = assocs.begin(); iter != assocs.end();)
    {
        if ((std::get<2>(*iter)).compare(path) == 0)
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
        if (intf.second->redundancyPriority)
        {
            if (intf.second->redundancyPriority.get()->priority() < value)
            {
                return false;
            }
        }
    }
    return true;
}

void ItemUpdater::updateUbootEnvVars(const std::string& versionId)
{
    auto it = versions.find(versionId);
    if (it == versions.end())
    {
        return;
    }
    auto flashId = it->second->path();
    helper.updateUbootVersionId(flashId);
}

void ItemUpdater::resetUbootEnvVars()
{
    decltype(activations.begin()->second->redundancyPriority.get()->priority())
        lowestPriority = std::numeric_limits<uint8_t>::max();
    decltype(activations.begin()->second->versionId) lowestPriorityVersion;
    for (const auto& intf : activations)
    {
        if (!intf.second->redundancyPriority.get())
        {
            // Skip this version if the redundancyPriority is not initialized.
            continue;
        }

        if (intf.second->redundancyPriority.get()->priority() <= lowestPriority)
        {
            lowestPriority = intf.second->redundancyPriority.get()->priority();
            lowestPriorityVersion = intf.second->versionId;
        }
    }

    // Update the U-boot environment variable to point to the lowest priority
    updateUbootEnvVars(lowestPriorityVersion);
}

void ItemUpdater::freeSpace([[maybe_unused]] const Activation& caller)
{
#ifdef BMC_STATIC_DUAL_IMAGE
    // For the golden image case, always remove the version on the primary side
    std::string versionIDtoErase;
    for (const auto& iter : activations)
    {
        if (iter.second->redundancyPriority &&
            iter.second->redundancyPriority.get()->priority() == 0)
        {
            versionIDtoErase = iter.second->versionId;
            break;
        }
    }
    if (!versionIDtoErase.empty())
    {
        erase(versionIDtoErase);
    }
    else
    {
        warning("Failed to find version to erase");
    }
#else
    //  Versions with the highest priority in front
    std::priority_queue<std::pair<int, std::string>,
                        std::vector<std::pair<int, std::string>>,
                        std::less<std::pair<int, std::string>>>
        versionsPQ;

    std::size_t count = 0;
    for (const auto& iter : activations)
    {
        if ((iter.second.get()->activation() ==
             server::Activation::Activations::Active) ||
            (iter.second.get()->activation() ==
             server::Activation::Activations::Failed))
        {
            count++;
            // Don't put the functional version on the queue since we can't
            // remove the "running" BMC version.
            // If ACTIVE_BMC_MAX_ALLOWED <= 1, there is only one active BMC,
            // so remove functional version as well.
            // Don't delete the the Activation object that called this function.
            if ((versions.find(iter.second->versionId)
                     ->second->isFunctional() &&
                 ACTIVE_BMC_MAX_ALLOWED > 1) ||
                (iter.second->versionId == caller.versionId))
            {
                continue;
            }

            // Failed activations don't have priority, assign them a large value
            // for sorting purposes.
            auto priority = 999;
            if (iter.second.get()->activation() ==
                    server::Activation::Activations::Active &&
                iter.second->redundancyPriority)
            {
                priority = iter.second->redundancyPriority.get()->priority();
            }

            versionsPQ.push(std::make_pair(priority, iter.second->versionId));
        }
    }

    // If the number of BMC versions is over ACTIVE_BMC_MAX_ALLOWED -1,
    // remove the highest priority one(s).
    while ((count >= ACTIVE_BMC_MAX_ALLOWED) && (!versionsPQ.empty()))
    {
        erase(versionsPQ.top().second);
        versionsPQ.pop();
        count--;
    }
#endif
}

void ItemUpdater::mirrorUbootToAlt()
{
    helper.mirrorAlt();
}

bool ItemUpdater::checkImage(const std::string& filePath,
                             const std::vector<std::string>& imageList)
{
    bool valid = true;

    for (auto& bmcImage : imageList)
    {
        fs::path file(filePath);
        file /= bmcImage;
        std::ifstream efile(file.c_str());
        if (efile.good() != 1)
        {
            valid = false;
            break;
        }
    }

    return valid;
}

#ifdef HOST_BIOS_UPGRADE
void ItemUpdater::createBIOSObject()
{
    std::string path = BIOS_OBJPATH;
    // Get version id from last item in the path
    auto pos = path.rfind("/");
    if (pos == std::string::npos)
    {
        error("No version id found in object path {PATH}", "PATH", path);
        return;
    }

    createActiveAssociation(path);
    createFunctionalAssociation(path);
    createUpdateableAssociation(path);

    auto versionId = path.substr(pos + 1);
    auto version = "null";
    AssociationList assocs = {};
    biosActivation = std::make_unique<Activation>(
        bus, path, *this, versionId, server::Activation::Activations::Active,
        assocs);
    auto dummyErase = [](std::string /*entryId*/) {
        // Do nothing;
    };
    biosVersion = std::make_unique<VersionClass>(
        bus, path, version, VersionPurpose::Host, "", "",
        std::vector<std::string>(),
        std::bind(dummyErase, std::placeholders::_1), "");
    biosVersion->deleteObject =
        std::make_unique<phosphor::software::manager::Delete>(bus, path,
                                                              *biosVersion);
}
#endif

void ItemUpdater::getRunningSlot()
{
    // Check /run/media/slot to get the slot number
    constexpr auto slotFile = "/run/media/slot";
    std::fstream f(slotFile, std::ios_base::in);
    f >> runningImageSlot;
}

} // namespace updater
} // namespace software
} // namespace phosphor
