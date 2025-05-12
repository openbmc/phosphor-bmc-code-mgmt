#include "item_updater_helper.hpp"

#include "utils.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

Helper::Helper(sdbusplus::bus_t& bus) : bus(bus) {}

void Helper::factoryReset()
{
    // Set openbmconce=factory-reset env in U-Boot.
    // The init will cleanup rwfs during boot.
    utils::execute("/sbin/fw_setenv", "openbmconce", "factory-reset");
}

void Helper::removeVersion(const std::string& /* flashId */)
{
    // Empty
}

void Helper::updateUbootVersionId(const std::string& /* flashId */)
{
    // Empty
}

void Helper::mirrorAlt()
{
    // Empty
}

} // namespace updater
} // namespace software
} // namespace phosphor
