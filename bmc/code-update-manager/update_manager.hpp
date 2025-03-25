#pragma once

#include "../../common/include/software.hpp"
#include "../../common/include/software_manager.hpp"
#include "../../common/include/utils.hpp"

#include <unistd.h>

#include <sdbusplus/async/context.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/ApplyTime/server.hpp>
#include <xyz/openbmc_project/Software/Manager/server.hpp>
#include <xyz/openbmc_project/Software/Update/aserver.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace phosphor
{
namespace software
{

using RequestedApplyTimes = sdbusplus::xyz::openbmc_project::Software::server::
    ApplyTime::RequestedApplyTimes;

using ActivationStatus =
    sdbusplus::xyz::openbmc_project::Software::server::Activation::Activations;

/**
 * @brief A simple RAII wrapper around a file descriptor.
 */
/**
 * @brief UpdateManager class.
 *
 * Inherits from:
 *  - @c SoftwareManager: Common code for managing software
 *  - @c xyz.openbmc_project.Software.Manager: D-Bus interface for software
 * manager
 *  - @c xyz.openbmc_project.Software.Update:  D-Bus interface for starting
 * updates
 *
 * Provides functionality to:
 *  - Accept an update image
 *  - Delegate updates to PLDM services (if applicable)
 *  - Track activation states and progress
 */
class UpdateManager :
    public phosphor::software::manager::SoftwareManager,
    public sdbusplus::server::object_t<
        sdbusplus::xyz::openbmc_project::Software::server::Manager>,
    public sdbusplus::aserver::xyz::openbmc_project::software::Update<
        UpdateManager>
{
  public:
    UpdateManager(const UpdateManager&) = delete;
    UpdateManager& operator=(const UpdateManager&) = delete;
    UpdateManager(UpdateManager&&) = delete;
    UpdateManager& operator=(UpdateManager&&) = delete;
    /**
     * @brief Constructs the UpdateManager.
     *
     * @param[in] ctx The sdbusplus asynchronous context.
     */
    UpdateManager(sdbusplus::async::context& ctx);

    /**
     * @brief Default destructor.
     */
    ~UpdateManager() = default;

    /**
     * @brief The StartUpdate D-Bus method implementation.
     *
     * Accepts a file descriptor for the update image, along with the requested
     * apply time (e.g., OnReset, Immediate), and initiates the update flow.
     *
     * @param[in]  unused    Placeholder to match D-Bus method signature
     * @param[in]  image     File descriptor containing the update image
     * @param[in]  applyTime Requested apply time
     *
     * @return A D-Bus object path to the newly created software object.
     */
    auto method_call(start_update_t, sdbusplus::message::unix_fd image,
                     RequestedApplyTimes applyTime)
        -> sdbusplus::async::task<start_update_t::return_type>;

  protected:
    /**
     * @brief Device initialization stub (required by base class).
     *
     * @param[in] devicePath   Unused in this example.
     * @param[in] configPath   Unused in this example.
     * @param[in] config       Unused in this example.
     *
     * @return True on success, via a coroutine.
     */
    sdbusplus::async::task<bool> initDevice(
        const std::string& /*unused*/, const std::string& /*unused*/,
        phosphor::software::config::SoftwareConfig& /*unused*/) override
    {
        co_return true;
    }

  private:
    /**
     * @brief Internal helper coroutine to carry out the update.
     *
     * This may include steps such as forwarding the update image
     * to the PLDM service.
     *
     * @param[in] swObj     Shared pointer to the newly created software object
     * @param[in] fd        The update image file descriptor (wrapped in
     * UniqueFD)
     * @param[in] applyTime Requested apply time
     *
     * @return D-Bus path of the PLDM software object, or empty on failure.
     */
    sdbusplus::async::task<sdbusplus::message::object_path> doUpdate(
        std::shared_ptr<phosphor::software::Software> swObj, UniqueFD fd,
        RequestedApplyTimes applyTime);

    /**
     * @brief Optional call to the PLDM service to start the update process.
     *
     * @param[in] fd        The update image file descriptor
     * @param[in] applyTime Requested apply time
     *
     * @return The PLDM software object's path, or empty on failure.
     */
    sdbusplus::async::task<sdbusplus::message::object_path> startPLDMUpdate(
        UniqueFD fd, RequestedApplyTimes applyTime);

    /**
     * @brief Checks if the device is ready to be activated.
     *
     * Subscribes to progress signals, retrieves current activation status,
     * and may trigger handlePLDMActivation if ready.
     *
     * @param[in] path The D-Bus path to the software object.
     */
    sdbusplus::async::task<void> checkDeviceActivationStatus(
        const std::string& path);

    /**
     * @brief Subscribes to ActivationProgress signals for the specified path.
     *
     * Updates the software object's progress when signaled.
     *
     * @param[in] path D-Bus path to the software object.
     */
    void subscribeToProgressSignal(const std::string& path);

    /**
     * @brief Subscribes to Activation signals for the specified path.
     *
     * Handles state transitions such as Ready -> Activating -> Active.
     *
     * @param[in] path D-Bus path to the software object.
     */
    void subscribeToActivationSignal(const std::string& path);

    /**
     * @brief Handles new progress updates coming from the PLDM software.
     *
     * @param[in] path     D-Bus path of the software object.
     * @param[in] progress New progress percentage value.
     */
    void handlePLDMProgress(const std::string& path, int32_t progress);

    /**
     * @brief Handles changes in the PLDM activation state.
     *
     * @param[in] path   D-Bus path of the software object.
     * @param[in] status New activation state.
     */
    sdbusplus::async::task<void> handlePLDMActivation(const std::string& path,
                                                      ActivationStatus status);

    /**
     * @struct SWHolder
     * @brief A wrapper struct to hold shared pointers to Software objects.
     */
    struct SWHolder
    {
        std::shared_ptr<phosphor::software::Software> sw;
    };

    /**
     * @brief Map of paths to SWHolder objects tracking currently active
     * software updates.
     */
    std::map<std::string, SWHolder> activeSW;

    /**
     * @brief Holds match objects for subscribed D-Bus signals.
     */
    std::vector<std::unique_ptr<sdbusplus::bus::match_t>> signalMatches;
};

} // namespace software
} // namespace phosphor
