#include "update_manager.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace phosphor
{
namespace software
{

// Minimal constructor for our UpdateManager.
UpdateManager::UpdateManager(sdbusplus::async::context& ctxIn) :
    SoftwareManager(ctxIn, "Update.Manager"),
    sdbusplus::server::object_t<
        sdbusplus::xyz::openbmc_project::Software::server::Manager>(
        ctxIn.get_bus(), "/xyz/openbmc_project/software/manager")
{
    // Request our bus name
    ctxIn.get_bus().request_name("xyz.openbmc_project.Software.Update.Manager");
}

UpdateManager::~UpdateManager() = default;

// Required by base class but not used yet
sdbusplus::async::task<bool> UpdateManager::initDevice(
    const std::string& /*unused*/, const std::string& /*unused*/,
    phosphor::software::config::SoftwareConfig& /*unused*/)
{
    co_return true;
}

} // namespace software
} // namespace phosphor
