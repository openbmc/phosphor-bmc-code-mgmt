#pragma once

#include "config.h"

#include <sdbusplus/async.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <random>
#include <string>
#include <tuple>

namespace phosphor::software::updater
{
class ItemUpdater;
}

namespace phosphor::software::update
{

using UpdateIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Update>;
using ItemUpdaterIntf = phosphor::software::updater::ItemUpdater;

using ApplyTimeIntf =
    sdbusplus::common::xyz::openbmc_project::software::ApplyTime;

/** @class Manager
 *  @brief Processes the image file from update D-Bus interface.
 *  @details The update manager class handles software updates and manages
 * software info through version and activation objects.
 */
class Manager : public UpdateIntf
{
  public:
    /** @brief Constructs Manager Class
     *
     * @param[in] bus - The Dbus bus object
     */
    explicit Manager(sdbusplus::async::context& ctx, const std::string& path,
                     ItemUpdaterIntf& itemUpdater) :
        UpdateIntf(ctx.get_bus(), path.c_str(), UpdateIntf::action::defer_emit),
        ctx(ctx), itemUpdater(itemUpdater)
    {
        emit_object_added();
    }

  private:
    /** @brief Implementation for StartUpdate
     *  Start a firware update to be performed asynchronously.
     */
    sdbusplus::message::object_path startUpdate(
        sdbusplus::message::unix_fd image,
        ApplyTimeIntf::RequestedApplyTimes applyTime) override;

    /* @brief Process the image supplied via image fd */
    auto processImage(sdbusplus::message::unix_fd image,
                      ApplyTimeIntf::RequestedApplyTimes applyTime,
                      std::string id, std::string objPath)
        -> sdbusplus::async::task<>;

    /* @brief The handler for the image processing failure  */
    void processImageFailed(sdbusplus::message::unix_fd image, std::string& id);

    /** @brief The random generator for the software id */
    std::mt19937 randomGen{static_cast<unsigned>(
        std::chrono::system_clock::now().time_since_epoch().count())};

    /** @brief D-Bus context */
    sdbusplus::async::context& ctx;
    /** @brief item_updater reference */
    ItemUpdaterIntf& itemUpdater;
    /** @brief State whether update is in progress */
    bool updateInProgress = false;
};

} // namespace phosphor::software::update
