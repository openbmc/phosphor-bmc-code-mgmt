#include "software.hpp"

#include "device.hpp"
#include "software_update.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/aserver.hpp>
#include <xyz/openbmc_project/Software/Update/aserver.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

Software::Software(sdbusplus::async::context& ctx, const char* objPathConfig,
                   Device& parent) :
    Software(ctx, objPathConfig, Software::getRandomSoftwareId(parent), parent)
{}

Software::Software(sdbusplus::async::context& ctx, const char* objPathConfig,
                   const std::string& softwareId, Device& parent) :
    Software(ctx, Software::getObjPathFromSwid(softwareId), objPathConfig,
             softwareId, parent)
{}

Software::Software(sdbusplus::async::context& ctx, std::string objPath,
                   const char* objPathConfig, const std::string& softwareId,
                   Device& parent) :
    sdbusplus::aserver::xyz::openbmc_project::software::Activation<Software>(
        ctx, objPath.c_str()),
    sdbusplus::aserver::xyz::openbmc_project::software::Version<Software>(
        ctx, objPath.c_str()),
    sdbusplus::aserver::xyz::openbmc_project::association::Definitions<
        Software>(ctx, objPath.c_str()),
    swid(softwareId), parent(parent), ctx(ctx)
{
    lg2::debug("creating dbus interfaces for swid {SWID} on path {OBJPATH}",
               "SWID", softwareId, "OBJPATH", objPath);

    lg2::info(
        "(TODO) create functional association from Version to Inventory Item");

    // we can associate to our configuration item, as discussed,
    // since the inventory item may be unclear/unavailable
    std::string forward = "config"; // we forward associate to our configuration
    std::string reverse = "software";
    std::string endpoint =
        std::string(objPathConfig); // our configuration object path

    this->associations({{forward, reverse, endpoint}});

    this->version("unknown");

    // This one seems to be required.
    // Otherwise the new software does not show up in the redfish fw inventory.
    sdbusplus::aserver::xyz::openbmc_project::software::Version<
        Software>::emit_added();
};

std::string Software::getObjPathFromSwid(const std::string& swid)
{
    std::string basepath = "/xyz/openbmc_project/software/";
    return basepath + swid;
}

std::string Software::getRandomSoftwareId(Device& parent)
{
    const std::string configType = parent.getEMConfigType();
    return std::format("{}_{}", configType, getRandomId());
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
