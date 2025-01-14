#include "software.hpp"

#include "device.hpp"
#include "software_update.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/aserver.hpp>
#include <xyz/openbmc_project/Software/Update/aserver.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software;
using namespace phosphor::software::device;
using namespace phosphor::software::config;
using namespace phosphor::software::update;

const static std::string baseObjPathSoftware = "/xyz/openbmc_project/software/";

SoftwareActivationProgress::SoftwareActivationProgress(
    sdbusplus::async::context& ctx, const char* objPath) :
    ActivationProgress(ctx, objPath)
{
    // This prevents "Conditional jump or move depends on uninitialised
    // value(s)"
    // when properties are updated for the first time
    progress_ = 0;
}

void SoftwareActivationProgress::setProgress(int progressArg)
{
    progress(progressArg);
}

Software::Software(sdbusplus::async::context& ctx, Device& parent) :
    Software(ctx, parent, getRandomSoftwareId(parent))
{}

Software::Software(sdbusplus::async::context& ctx, Device& parent,
                   const std::string& swid) :
    SoftwareActivation(ctx, (baseObjPathSoftware + swid).c_str()),
    objectPath(baseObjPathSoftware + swid), parentDevice(parent), swid(swid),
    ctx(ctx)
{
    // initialize the members of our base class to prevent
    // "Conditional jump or move depends on uninitialised value(s)"
    activation_ = Activations::NotReady;
    requested_activation_ = RequestedActivations::None;

    std::string objPath = baseObjPathSoftware + swid;

    debug("{SWID}: created dbus interfaces on path {OBJPATH}", "SWID", swid,
          "OBJPATH", objPath);
};

static long int getRandomId()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    unsigned int seed = ts.tv_nsec ^ getpid();
    srandom(seed);
    return random() % 10000;
}

std::string Software::getRandomSoftwareId(Device& parent)
{
    return std::format("{}_{}", parent.config.configName, getRandomId());
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<> Software::createInventoryAssociations(bool isRunning)
// NOLINTEND(readability-static-accessed-through-instance)
{
    debug("{SWID}: setting association definitions", "SWID", swid);

    std::string endpoint = "";

    try
    {
        endpoint = co_await parentDevice.config.getInventoryItemObjectPath(ctx);
    }
    catch (std::exception& e)
    {
        error(e.what());
    }

    if (!associationDefinitions)
    {
        std::string path = objectPath;
        associationDefinitions =
            std::make_unique<SoftwareAssociationDefinitions>(ctx, path.c_str());
    }

    std::vector<std::tuple<std::string, std::string, std::string>> assocs;

    if (endpoint.empty())
    {
        associationDefinitions->associations(assocs);
        co_return;
    }

    if (isRunning)
    {
        debug("{SWID}: creating 'running' association to {OBJPATH}", "SWID",
              swid, "OBJPATH", endpoint);
        std::tuple<std::string, std::string, std::string> assocRunning = {
            "running", "ran_on", endpoint};
        assocs.push_back(assocRunning);
    }
    else
    {
        debug("{SWID}: creating 'activating' association to {OBJPATH}", "SWID",
              swid, "OBJPATH", endpoint);
        std::tuple<std::string, std::string, std::string> assocActivating = {
            "activating", "activated_on", endpoint};
        assocs.push_back(assocActivating);
    }

    associationDefinitions->associations(assocs);

    co_return;
}

void Software::setVersion(const std::string& versionStr)
{
    debug("{SWID}: set version {VERSION}", "SWID", swid, "VERSION", versionStr);

    if (version)
    {
        error("{SWID}: version was already set", "SWID", swid);
        return;
    }

    version = std::make_unique<SoftwareVersion>(ctx, objectPath.str.c_str());
    version->version(versionStr);
}

void Software::setActivationBlocksTransition(bool enabled)
{
    if (!enabled)
    {
        activationBlocksTransition = nullptr;
        return;
    }

    std::string path = objectPath;
    activationBlocksTransition =
        std::make_unique<SoftwareActivationBlocksTransition>(ctx, path.c_str());
}

void Software::setActivation(SoftwareActivation::Activations act)
{
    activation(act);
}

void Software::enableUpdate(
    const std::set<RequestedApplyTimes>& allowedApplyTimes)
{
    if (updateIntf != nullptr)
    {
        error("[Software] update of {OBJPATH} has already been enabled",
              "OBJPATH", objectPath);
        return;
    }

    info(
        "[Software] enabling update of {OBJPATH} (adding the update interface)",
        "OBJPATH", objectPath);

    updateIntf = std::make_unique<SoftwareUpdate>(ctx, objectPath.str.c_str(),
                                                  *this, allowedApplyTimes);
}
