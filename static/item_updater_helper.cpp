#include "item_updater_helper.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <experimental/filesystem>

namespace phosphor
{
namespace software
{
namespace updater
{
using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

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
    // Factory reset cleans up all the contents in rwfs
    namespace fs = std::experimental::filesystem;
    constexpr auto rwDir = "/run/initramfs/rw/cow";

    if (!fs::is_directory(rwDir))
    {
        // The rwDir is expected to be a dir,
        // otherwise we have problem and let it throw
        log<level::ERR>("Unpexted rw dir");
        elog<InternalFailure>();
    }

    for (const auto& iter : fs::directory_iterator(rwDir))
    {
        fs::remove_all(iter);
    }
    log<level::INFO>("rwfs is cleaned up");
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
