#include "software.hpp"

#include "device.hpp"
#include "software_update.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/aserver.hpp>
#include <xyz/openbmc_project/Software/Update/aserver.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

Software::Software(sdbusplus::async::context& ctx, Device& parent) :
    Software(ctx, Software::getRandomSoftwareId(parent), parent)
{}

Software::Software(sdbusplus::async::context& ctx,
                   const std::string& softwareId, Device& parent) :
    Software(ctx, Software::getObjPathFromSwid(softwareId), softwareId, parent)
{}

Software::Software(sdbusplus::async::context& ctx, std::string objPath,
                   const std::string& softwareId, Device& parent) :
    sdbusplus::aserver::xyz::openbmc_project::software::Activation<Software>(
        ctx, objPath.c_str()),
    swid(softwareId), parent(parent), ctx(ctx)
{
    if (!objPath.starts_with("/"))
    {
        throw std::invalid_argument(objPath + " is not an object path");
    }

    lg2::debug("{SWID}: created dbus interfaces on path {OBJPATH}", "SWID",
               softwareId, "OBJPATH", objPath);
};

std::string Software::getObjPathFromSwid(const std::string& swid)
{
    std::string basepath = "/xyz/openbmc_project/software/";
    return basepath + swid;
}

std::string Software::getRandomSoftwareId(Device& parent)
{
    // design: Swid = <DeviceX>_<RandomId>
    // design: For same type devices, extend the Dbus path to specify device
    // instance, for example,
    // /xyz/openbmc_project/Software/<deviceX>_<InstanceNum>_<SwId>

    // The problem here is that InstanceNum needs to always stay the same,
    // so that the device can be identified in the redfish fw inventory.

    // Since 'Name' property is already provided in EM config, we can insert
    // that in place of 'InstanceNum'.

    const std::string configType = parent.getEMConfigType();

    // 'Name' property in EM config
    const std::string nameEM = parent.config.name;

    return std::format("{}_{}_{}", configType, nameEM, getRandomId());
}

long int Software::getRandomId()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    unsigned int seed = ts.tv_nsec ^ getpid();
    srandom(seed);
    return random() % 10000;
}

Device& Software::getParentDevice()
{
    return this->parent;
}

// NOLINTBEGIN
sdbusplus::async::task<> Software::setAssociationDefinitionsRunningActivating(
    bool isRunning, bool isActivating)
// NOLINTEND
{
    lg2::debug("{SWID}: setting association definitions", "SWID", this->swid);

    std::string endpoint = co_await parent.getInventoryItemObjectPath();

    if (!this->optSoftwareAssociationDefinitions)
    {
        std::string path = this->getObjectPath();
        this->optSoftwareAssociationDefinitions =
            std::make_unique<SoftwareAssociationDefinitions>(ctx, path.c_str());
    }

    std::string forward;
    std::string reverse;
    std::vector<std::tuple<std::string, std::string, std::string>> assocs;

    if (isRunning)
    {
        lg2::debug("{SWID}: creating 'running' association to {OBJPATH}",
                   "SWID", this->swid, "OBJPATH", endpoint);
        forward = "running";
        reverse = "ran_on";
        std::tuple<std::string, std::string, std::string> assocRunning = {
            forward, reverse, endpoint};
        assocs.push_back(assocRunning);
    }

    if (isActivating)
    {
        lg2::debug("{SWID}: creating 'activating' association to {OBJPATH}",
                   "SWID", this->swid, "OBJPATH", endpoint);
        forward = "activating";
        reverse = "activated_on";
        std::tuple<std::string, std::string, std::string> assocActivating = {
            forward, reverse, endpoint};
        assocs.push_back(assocActivating);
    }

    this->optSoftwareAssociationDefinitions->associations(assocs);

    co_return;
}

void Software::setVersion(const std::string& versionStr)
{
    lg2::debug("{SWID}: set version {VERSION}", "SWID", this->swid, "VERSION",
               versionStr);

    if (this->optSoftwareVersion)
    {
        lg2::error("{SWID}: version was already set", "SWID", this->swid);
        return;
    }

    std::string path = this->getObjectPath();
    this->optSoftwareVersion =
        std::make_unique<SoftwareVersion>(ctx, path.c_str());
    this->optSoftwareVersion->version(versionStr);
}

void Software::setActivationBlocksTransition(bool enabled)
{
    if (!enabled)
    {
        this->optActivationBlocksTransition = nullptr;
        return;
    }

    std::string path = this->getObjectPath();
    this->optActivationBlocksTransition =
        std::make_unique<sdbusplus::aserver::xyz::openbmc_project::software::
                             ActivationBlocksTransition<Software>>(
            this->ctx, path.c_str());
}

void Software::setActivation(
    sdbusplus::common::xyz::openbmc_project::software::Activation::Activations
        act)
{
    this->activation(act);
}

sdbusplus::message::object_path Software::getObjectPath() const
{
    std::string objPathStr = Software::getObjPathFromSwid(swid);
    return sdbusplus::message::object_path(objPathStr.c_str());
}

void Software::enableUpdate(
    const std::set<RequestedApplyTimes>& allowedApplyTimes)
{
    std::string objPath = getObjectPath();

    if (this->optSoftwareUpdate != nullptr)
    {
        lg2::error("[Software] update of {OBJPATH} has already been enabled",
                   "OBJPATH", objPath);
        return;
    }

    lg2::info(
        "[Software] enabling update of {OBJPATH} (adding the update interface)",
        "OBJPATH", objPath);

    optSoftwareUpdate = std::make_unique<SoftwareUpdate>(
        this->ctx, objPath.c_str(), *this, allowedApplyTimes);
}
