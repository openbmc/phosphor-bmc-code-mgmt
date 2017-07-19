#include "activation.hpp"
#include "item_updater.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

constexpr auto SYSTEMD_SERVICE   = "org.freedesktop.systemd1";
constexpr auto SYSTEMD_PATH      = "/org/freedesktop/systemd1";
constexpr auto SIGNAL_INTERFACE  = "/org/freedesktop/systemd1";
constexpr auto MANAGER_INTERFACE = "org.freedesktop.systemd1.Manager";

void Activation::subscribeToSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_SERVICE,
                                            SYSTEMD_PATH,
                                            SIGNAL_INTERFACE,
                                            "Subscribe");
    this->bus.call_noreply(method);

    return;
}

auto Activation::activation(Activations value) ->
        Activations
{

    if (value != softwareServer::Activation::Activations::Active)
    {
        redundancyPriority.reset(nullptr);
    }

    if (value == softwareServer::Activation::Activations::Activating)
    {
        if (rwVolumeCreated == false && roVolumeCreated == false)
        {
            if (!activationBlocksTransition)
            {
                activationBlocksTransition =
                          std::make_unique<ActivationBlocksTransition>(
                                    bus,
                                    path);
            }

            auto method = bus.new_method_call(
                    SYSTEMD_SERVICE,
                    SYSTEMD_PATH,
                    MANAGER_INTERFACE,
                    "StartUnit");
            method.append("obmc-flash-bmc-ubirw.service", "replace");
            bus.call_noreply(method);

            auto roServiceFile = "obmc-flash-bmc-ubiro@" + versionId +
                    ".service";
            method = bus.new_method_call(
                    SYSTEMD_SERVICE,
                    SYSTEMD_PATH,
                    MANAGER_INTERFACE,
                    "StartUnit");
            method.append(roServiceFile, "replace");
            bus.call_noreply(method);
        }
        else if (rwVolumeCreated == true && roVolumeCreated == true)
        {
            if (!redundancyPriority)
            {
                redundancyPriority =
                          std::make_unique<RedundancyPriority>(
                                    bus,
                                    path,
                                    *this,
                                    0);
            }

            activationBlocksTransition.reset(nullptr);
            return softwareServer::Activation::activation(
                    softwareServer::Activation::Activations::Active);
        }
    }
    else
    {
        activationBlocksTransition.reset(nullptr);
    }
    return softwareServer::Activation::activation(value);
}

auto Activation::requestedActivation(RequestedActivations value) ->
        RequestedActivations
{
    rwVolumeCreated = false;
    roVolumeCreated = false;

    if ((value == softwareServer::Activation::RequestedActivations::Active) &&
        (softwareServer::Activation::requestedActivation() !=
                  softwareServer::Activation::RequestedActivations::Active))
    {
        if ((softwareServer::Activation::activation() ==
                    softwareServer::Activation::Activations::Ready) ||
            (softwareServer::Activation::activation() ==
                    softwareServer::Activation::Activations::Failed))
        {
            Activation::activation(
                    softwareServer::Activation::Activations::Activating);

        }
    }
    return softwareServer::Activation::requestedActivation(value);
}

uint8_t RedundancyPriority::priority(uint8_t value)
{
    parent.parent.freePriority(value);
    return softwareServer::RedundancyPriority::priority(value);
}

void Activation::unitStateChange(sdbusplus::message::message& msg)
{
    uint32_t newStateID {};
    sdbusplus::message::object_path newStateObjPath;
    std::string newStateUnit{};
    std::string newStateResult{};

    //Read the msg and populate each variable
    msg.read(newStateID, newStateObjPath, newStateUnit, newStateResult);

    auto rwServiceFile = "obmc-flash-bmc-ubirw.service";
    auto roServiceFile = "obmc-flash-bmc-ubiro@" + versionId + ".service";

    if(newStateUnit == rwServiceFile && newStateResult == "done")
    {
        rwVolumeCreated = true;
    }

    if(newStateUnit == roServiceFile && newStateResult == "done")
    {
        roVolumeCreated = true;
    }

    if(rwVolumeCreated && roVolumeCreated)
    {
        Activation::activation(
                softwareServer::Activation::Activations::Activating);
    }

    if((newStateUnit == rwServiceFile || newStateUnit == roServiceFile) &&
        (newStateResult == "failed" || newStateResult == "dependency"))
    {
        Activation::activation(softwareServer::Activation::Activations::Failed);
    }

    return;
}


} // namespace updater
} // namespace software
} // namespace phosphor
