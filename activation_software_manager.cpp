#include "activation_software_manager.hpp"
#include <log.hpp>

namespace phosphor
{
namespace software
{
namespace manager
{

using namespace phosphor::logging;

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Software::server;

Activation::Activations Activation::activation(Activations value)
{
    log<level::INFO>("Change to Software Activation",
                     entry("ACTIVATION=%s",
                           convertForMessage(value).c_str()));

    return server::Activation::activation(value);
}

Activation::RequestedActivations Activation::requestedActivation(
    RequestedActivations value)
{
    log<level::INFO>("Change to Software Requested Activation",
                     entry("REQUESTEDACTIVATION=%s",
                           convertForMessage(value).c_str()));

    return server::Activation::requestedActivation(value);
}

} // namespace manager
} // namespace state
} // namepsace phosphor

