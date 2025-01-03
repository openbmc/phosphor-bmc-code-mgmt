#include "item_updater_helper.hpp"

#include "utils.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

void Helper::setEntry(const std::string& /* entryId */, uint8_t /* value */)
{
    // Empty
}

void Helper::clearEntry(const std::string& /* entryId */)
{
    // Empty
}

void Helper::cleanup()
{
    // Empty
}

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
