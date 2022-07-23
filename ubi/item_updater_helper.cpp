#include "config.h"

#include "item_updater_helper.hpp"

#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/exception.hpp>

namespace phosphor
{
namespace software
{
namespace updater
{

PHOSPHOR_LOG2_USING;

void Helper::setEntry(const std::string& entryId, uint8_t value)
{
    std::string serviceFile = "obmc-flash-bmc-setenv@" + entryId + "\\x3d" +
                              std::to_string(value) + ".service";
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(serviceFile, "replace");
    bus.call_noreply(method);
}

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
    utils::execute("/sbin/fw_setenv", "rwreset", "true");
}

void Helper::removeVersion(const std::string& flashId)
{
    auto serviceFile = "obmc-flash-bmc-ubiro-remove@" + flashId + ".service";

    // Remove the read-only partitions.
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(serviceFile, "replace");
    bus.call_noreply(method);
}

void Helper::updateUbootVersionId(const std::string& flashId)
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    auto updateEnvVarsFile =
        "obmc-flash-bmc-updateubootvars@" + flashId + ".service";
    method.append(updateEnvVarsFile, "replace");

    try
    {
        bus.call_noreply(method);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Failed to update u-boot env variables: {FLASHID}", "FLASHID",
              flashId);
    }
}

void Helper::mirrorAlt()
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    auto mirrorUbootFile = "obmc-flash-bmc-mirroruboot.service";
    method.append(mirrorUbootFile, "replace");

    try
    {
        bus.call_noreply(method);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Failed to copy U-Boot to alternate chip: {ERROR}", "ERROR", e);
    }
}

} // namespace updater
} // namespace software
} // namespace phosphor
