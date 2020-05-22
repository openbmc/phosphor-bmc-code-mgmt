#include "config.h"

#include "item_updater_helper.hpp"

#include <filesystem>

namespace phosphor
{
namespace software
{
namespace updater
{

namespace fs = std::filesystem;

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
    // Empty
}

void Helper::removeVersion(const std::string& versionId)
{
    // Remove the kernel image file
    fs::remove(fs::path(BMC_KERNEL_PREFIX + versionId));
}

void Helper::updateUbootVersionId(const std::string& versionId)
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    auto serviceFile = "obmc-flash-bmc-mmc-setprimary@" + versionId + ".service";
    method.append(serviceFile, "replace");
    bus.call_noreply(method);
}

void Helper::mirrorAlt()
{
    // Empty
}

} // namespace updater
} // namespace software
} // namespace phosphor
