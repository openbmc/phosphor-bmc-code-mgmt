#include "item_updater_helper.hpp"

#include <phosphor-logging/log.hpp>

#include <cstdlib>

namespace phosphor
{
namespace software
{
namespace updater
{
using namespace phosphor::logging;

void Helper::clearEntry(const std::string& entryId)
{
    // Empty
}

void Helper::cleanup()
{
    // Empty
}

void Helper::factoryReset()
{
    // Set openbmconce=factory-reset env in u-boot,
    // The init will cleanup rwfs during boot.
    constexpr auto cmd = "fw_setenv openbmconce factory-reset";

    // Set the variable twice so that it is written to both environments of
    // the u-boot redundant environment variables since initramfs can only
    // read one of the environments.
    auto r = std::system(cmd);
    r |= std::system(cmd);
    if (r != 0)
    {
        log<level::ERR>("Failed to set factory-reset uboot env");
    }
}

void Helper::removeVersion(const std::string& versionId)
{
    // Empty
}

void Helper::updateUbootVersionId(const std::string& versionId)
{
    // Empty
}

void Helper::enableFieldMode()
{
    // TODO
}

void Helper::mirrorAlt()
{
    // Empty
}

} // namespace updater
} // namespace software
} // namespace phosphor
