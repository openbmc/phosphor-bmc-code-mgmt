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

void Activation::setInitialProperties(Activations activation)
{
    server::Activation::activation(activation);

    return;
}

Activation::Activations Activation::activation(Activations value)
{
    log<level::INFO>("Change to Software Activation",
                     entry("ACTIVATION=%s",
                           convertForMessage(value).c_str()));

    return server::Activation::activation(value);
}

} // namespace manager
} // namespace state
} // namepsace phosphor

