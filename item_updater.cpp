#include <string>
#include "item_updater.hpp"
#include "config.h"

namespace phosphor
{
namespace software
{
namespace updater
{

int ItemUpdater::createActivation(sd_bus_message* msg,
                                  void* userData,
                                  sd_bus_error* retErr)
{
    auto versionId = 1;
    auto* updater = static_cast<ItemUpdater*>(userData);
    updater->activations.insert(std::make_pair(
            std::to_string(versionId),
            std::make_unique<Activation>(
                    updater->busItem,
                    SOFTWARE_OBJPATH)));
    return 0;
}

} // namespace updater
} // namespace software
} // namespace phosphor
