#pragma once

#include "../../common/include/software_manager.hpp"

#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Software/Manager/server.hpp>

namespace phosphor
{
namespace software
{

/**
 * @class UpdateManager
 * @brief Minimal skeleton for the new code-update-manager
 *
 * For now, we only inherit from:
 *  - SoftwareManager: common code for software mgmt
 *  - xyz.openbmc_project.Software.Manager: the D-Bus interface
 *
 * We'll add more advanced DBus functions in a subsequent commit.
 */
class UpdateManager :
    public manager::SoftwareManager,
    public sdbusplus::server::object_t<
        sdbusplus::xyz::openbmc_project::Software::server::Manager>
{
  public:
    UpdateManager(const UpdateManager&) = delete;
    UpdateManager& operator=(const UpdateManager&) = delete;
    UpdateManager(UpdateManager&&) = delete;
    UpdateManager& operator=(UpdateManager&&) = delete;

    /**
     * @brief Constructs the UpdateManager.
     *
     * @param[in] ctxIn - The sdbusplus asynchronous context
     */
    explicit UpdateManager(sdbusplus::async::context& ctxIn);

    /**
     * @brief Destructor
     */
    ~UpdateManager();

  protected:
    /**
     * @brief Device initialization stub (required by base class).
     *
     * @return True on success (co_return).
     */
    sdbusplus::async::task<bool> initDevice(
        const std::string& devicePath, const std::string& configPath,
        config::SoftwareConfig& config) override;
};

} // namespace software
} // namespace phosphor
