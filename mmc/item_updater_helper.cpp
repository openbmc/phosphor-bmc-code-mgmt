#include "config.h"

#include "item_updater_helper.hpp"

#include "utils.hpp"

#include <thread>

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
    // Mark the read-write partition for recreation upon reboot.
    utils::execute("/sbin/fw_setenv", "rwreset", "true");
}

void Helper::removeVersion(const std::string& flashId)
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    auto serviceFile = "obmc-flash-mmc-remove@" + flashId + ".service";
    method.append(serviceFile, "replace");
    bus.call_noreply(method);

    // Wait a few seconds for the service file to finish, otherwise the BMC may
    // start the update while the image is still being deleted.
    constexpr auto removeWait = std::chrono::seconds(3);
    std::this_thread::sleep_for(removeWait);
}

void Helper::updateUbootVersionId(const std::string& flashId)
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    auto serviceFile = "obmc-flash-mmc-setprimary@" + flashId + ".service";
    method.append(serviceFile, "replace");
    bus.call_noreply(method);

    // Wait a few seconds for the service file to finish, otherwise the BMC may
    // be rebooted while pointing to a non-existent version.
    constexpr auto setPrimaryWait = std::chrono::seconds(3);
    std::this_thread::sleep_for(setPrimaryWait);
}

void Helper::mirrorAlt()
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    auto serviceFile = "obmc-flash-mmc-mirroruboot.service";
    method.append(serviceFile, "replace");
    bus.call_noreply(method);
}

} // namespace updater
} // namespace software
} // namespace phosphor
