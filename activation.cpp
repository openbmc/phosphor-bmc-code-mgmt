#include "activation.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

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
    return softwareServer::Activation::requestedActivation(value);
}

} // namespace updater
} // namespace software
} // namespace phosphor
