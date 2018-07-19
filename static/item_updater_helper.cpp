#include "config.h"
#include "item_updater_helper.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

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
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(
        "obmc-flash-bmc-setenv@openbmconce\\x3dfactory\\x2dreset.service",
        "replace");
    bus.call_noreply(method);
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
