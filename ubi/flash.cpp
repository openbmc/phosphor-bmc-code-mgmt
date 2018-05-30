#include "config.h"
#include "flash.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

void Flash::write()
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("obmc-flash-bmc-ubirw.service", "replace");
    bus.call_noreply(method);

    auto roServiceFile = "obmc-flash-bmc-ubiro@" + versionId + ".service";
    method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                 SYSTEMD_INTERFACE, "StartUnit");
    method.append(roServiceFile, "replace");
    bus.call_noreply(method);

    return;
}

} // namespace updater
} // namespace software
} // namepsace phosphor
