#include <phosphor-logging/log.hpp>

#include "config.h"
#include "item_updater_helper.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

using namespace phosphor::logging;
void Helper::clearEntry(const std::string& entryId)
{
    // Remove the priority environment variable.
    auto serviceFile = "obmc-flash-bmc-setenv@" + entryId + ".service";
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(serviceFile, "replace");
    bus.call_noreply(method);
}

void Helper::cleanup()
{
    // Remove any volumes that do not match current versions.
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("obmc-flash-bmc-cleanup.service", "replace");
    bus.call_noreply(method);
}

void Helper::factoryReset()
{
    // Mark the read-write partition for recreation upon reboot.
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("obmc-flash-bmc-setenv@rwreset\\x3dtrue.service", "replace");
    bus.call_noreply(method);
}

void Helper::removeVersion(const std::string& versionId)
{
    auto serviceFile = "obmc-flash-bmc-ubiro-remove@" + versionId + ".service";

    // Remove the read-only partitions.
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(serviceFile, "replace");
    bus.call_noreply(method);
}

void Helper::updateUbootVersionId(const std::string& versionId)
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    auto updateEnvVarsFile =
        "obmc-flash-bmc-updateubootvars@" + versionId + ".service";
    method.append(updateEnvVarsFile, "replace");
    auto result = bus.call(method);

    // Check that the bus call didn't result in an error
    if (result.is_method_error())
    {
        log<level::ERR>("Failed to update u-boot env variables",
                        entry("VERSIONID=%s", versionId.c_str()));
    }
}

void Helper::enableFieldMode()
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("obmc-flash-bmc-setenv@fieldmode\\x3dtrue.service",
                  "replace");
    bus.call_noreply(method);

    method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                 SYSTEMD_INTERFACE, "StopUnit");
    method.append("usr-local.mount", "replace");
    bus.call_noreply(method);

    std::vector<std::string> usrLocal = {"usr-local.mount"};

    method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                 SYSTEMD_INTERFACE, "MaskUnitFiles");
    method.append(usrLocal, false, true);
    bus.call_noreply(method);
}
void Helper::mirrorAlt()
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    auto mirrorUbootFile = "obmc-flash-bmc-mirroruboot.service";
    method.append(mirrorUbootFile, "replace");
    auto result = bus.call(method);

    // Check that the bus call didn't result in an error
    if (result.is_method_error())
    {
        log<level::ERR>("Failed to copy U-Boot to alternate chip");
    }
}

} // namespace updater
} // namespace software
} // namespace phosphor
