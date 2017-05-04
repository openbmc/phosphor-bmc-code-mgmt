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
    auto mapper = updater->bus.new_method_call(
                      MAPPER_BUSNAME,
                      MAPPER_PATH,
                      MAPPER_INTERFACE,
                      "GetSubTree");
    mapper.append(SOFTWARE_OBJPATH,
                  1, // Depth
                  std::vector<std::string>({VERSION_IFACE}));

    auto mapperResponseMsg = updater->bus.call(mapper);
    if (mapperResponseMsg.is_method_error())
    {
        log<level::ERR>("Error in mapper call",
                        entry("PATH=%s", SOFTWARE_OBJPATH),
                        entry("INTERFACE=%s", VERSION_IFACE));
        return -1;
    }

    std::map<std::string, std::map<
    std::string, std::vector<std::string>>> mapperResponse;
    mapperResponseMsg.read(mapperResponse);
    if (mapperResponse.empty())
    {
        log<level::ERR>("Error reading mapper response",
                        entry("PATH=%s", SOFTWARE_OBJPATH),
                        entry("INTERFACE=%s", VERSION_IFACE));
        return -1;
    }

    for (const auto& resp : mapperResponse)
    {

        const auto& path = resp.first;
        const auto& service = resp.second.begin()->first;

        auto method = updater->bus.new_method_call(
                          service.c_str(),
                          path.c_str(),
                          "org.freedesktop.DBus.Properties",
                          "Get");

        method.append(VERSION_IFACE, "Purpose");

        auto methodResponseMsg = updater->bus.call(method);
        if (methodResponseMsg.is_method_error())
        {
            log<level::ERR>("Error in method call",
                            entry("PATH=%s", path.c_str()),
                            entry("INTERFACE=%s", VERSION_IFACE));
            return -1;
        }

        sdbusplus::message::variant<std::string> purpose;
        mapperResponseMsg.read(purpose);
        std::string purposeStr = sdbusplus::message::variant_ns::get <
                                 std::string > (purpose);

        // Only process the BMC images
        if ((purposeStr.empty()) ||
            (purposeStr.compare(convertForMessage(server::Version::
                                VersionPurpose::BMC).c_str())))
        {
            continue;
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

        // For now set all BMC code versions to active
        auto activationState = server::Activation::Activations::Active;

        if (updater->activations.find(versionId) == updater->activations.end())
        {
            updater->activations.insert(std::make_pair(
                                            versionId,
                                            std::make_unique<Activation>(
                                                updater->bus,
                                                path,
                                                versionId,
                                                activationState)));
        }
    }
    return 0;
}

} // namespace updater
} // namespace software
} // namespace phosphor
