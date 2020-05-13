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

void Helper::updateUbootVersionId(const std::string& /* versionId */)
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
