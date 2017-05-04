#include <string>
#include <phosphor-logging/log.hpp>
#include "config.h"
#include "item_updater.hpp"
#include "xyz/openbmc_project/Software/Version/server.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Software::server;

using namespace phosphor::logging;

int ItemUpdater::createActivation(sd_bus_message* msg,
                                  void* userData,
                                  sd_bus_error* retErr)
{
    auto* updater = static_cast<ItemUpdater*>(userData);
    auto m = sdbusplus::message::message(msg);

    sdbusplus::message::object_path objPath;
    std::map<std::string,
        std::map<std::string,
        sdbusplus::message::variant<std::string>>> interfaces;
    m.read(objPath, interfaces);
    std::string path(std::move(objPath));

    for (const auto& intf : interfaces)
    {
        if (intf.first.compare(VERSION_IFACE))
        {
            continue;
        }

        for (const auto& property : intf.second)
        {
            if (!property.first.compare("Purpose"))
            {
                // Only process the BMC images
                std::string value = sdbusplus::message::variant_ns::get <
                                    std::string > (property.second);
                if (value.compare(convertForMessage(server::Version::
                                                    VersionPurpose::
                                                    BMC).c_str()))
                {
                    return 0;
                }
            }
        }
    }

    // Version id is the last item in the path
    auto pos = path.rfind("/");
    if (pos == std::string::npos)
    {
        log<level::ERR>("No version id found in object path",
                        entry("OBJPATH=%s", path));
        return -1;
    }

    auto versionId = path.substr(pos + 1);

    if (updater->activations.find(versionId) == updater->activations.end())
    {
        // For now set all BMC code versions to active
        auto activationState = server::Activation::Activations::Active;

        updater->activations.insert(std::make_pair(
                                        versionId,
                                        std::make_unique<Activation>(
                                            updater->bus,
                                            path,
                                            versionId,
                                            activationState)));
    }
    return 0;
}

} // namespace updater
} // namespace software
} // namespace phosphor
