#include "config.h"

#include "item_updater_helper.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{
// openbmconce=clean-rwfs-filesystem factory-reset
#define ENV_FACTORY_RESET "openbmconce\\x3dfactory\\x2dreset"
#define ENV_ENABLE_FIELD_MODE "fieldmode\\x3dtrue"
#define SERVICE_FACTORY_RESET                                                  \
    "obmc-flash-bmc-setenv@" ENV_FACTORY_RESET ".service"
#define SERVICE_ENABLE_FIELD_MODE                                              \
    "obmc-flash-bmc-setenv@" ENV_ENABLE_FIELD_MODE ".service"

void Helper::setEntry(const std::string& entryId, uint8_t value)
{
    // Empty
}

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
    // Set openbmconce=factory-reset env in U-Boot.
    // The init will cleanup rwfs during boot.
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(SERVICE_FACTORY_RESET, "replace");
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
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(SERVICE_ENABLE_FIELD_MODE, "replace");
    bus.call_noreply(method);
}

void Helper::mirrorAlt()
{
    // Empty
}

} // namespace updater
} // namespace software
} // namespace phosphor
