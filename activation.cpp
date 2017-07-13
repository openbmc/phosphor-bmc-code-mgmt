#include "activation.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

constexpr auto SYSTEMD_SERVICE       = "org.freedesktop.systemd1";
constexpr auto SYSTEMD_OBJ_PATH      = "/org/freedesktop/systemd1";
constexpr auto SYSTEMD_INTERFACE     = "org.freedesktop.systemd1.Manager";

const std::map<const char*, const char*> REBOOT_HALT =
{
     {"REBOOT", "reboot-guard"},
     {"PRE-REBOOT", "start-reboot-guard"}
};

auto Activation::activation(Activations value) ->
        Activations
{
    if (value == softwareServer::Activation::Activations::Activating)
    {
        if (!activationBlocksTransition)
        {
            activationBlocksTransition =
                      std::make_unique<ActivationBlocksTransition>(
                                bus,
                                path);
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

void ActivationBlocksTransition::blockTransition()
{

    for(auto& service : REBOOT_HALT)
    {
    auto method = this->bus.new_method_call(SYSTEMD_SERVICE,
                                            SYSTEMD_OBJ_PATH,
                                            SYSTEMD_INTERFACE,
                                            "StartUnit");
    method.append(service.second, "replace");
    this->bus.call(method);
    }
}

} // namespace updater
} // namespace software
} // namespace phosphor
