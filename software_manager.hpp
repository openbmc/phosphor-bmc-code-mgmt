#pragma once

#include "config.h"

#include "item_updater.hpp"

#include <sdbusplus/async.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <random>
#include <string>
#include <tuple>

namespace phosphor::software::manager
{

using UpdateIntf = sdbusplus::xyz::openbmc_project::Software::server::Update;
using ItemUpdaterIntf = phosphor::software::updater::ItemUpdater;
using ActivationIntf = phosphor::software::updater::Activation;
using ApplyTimeIntf =
    sdbusplus::common::xyz::openbmc_project::software::ApplyTime;

/** @class Manager
 *  @brief Processes the image file from update D-Bus interface.
 *  @details The software manager class handles software updates and manages
 * software info through version and activation objects.
 */
class Manager : public UpdateIntf
{
  public:
    /** @brief Constructs Manager Class
     *
     * @param[in] bus - The Dbus bus object
     */
    explicit Manager(sdbusplus::async::context& ctx, std::string& path) :
        UpdateIntf(ctx.get_bus(), path.c_str()), ctx(ctx),
        itemUpdater(std::make_unique<ItemUpdaterIntf>(ctx, path, true))
    {}

  private:
    /** @brief Implementation for StartUpdate
     *  Start a firware update to be performed asynchronously.
     */
    virtual sdbusplus::message::object_path
        startUpdate(sdbusplus::message::unix_fd image,
                    ApplyTimeIntf::RequestedApplyTimes applyTime) override;

    /* @brief Process the image supplied via image fd */
    auto processImage(sdbusplus::message::unix_fd image,
                      ApplyTimeIntf::RequestedApplyTimes applyTime,
                      std::string id, sdbusplus::message::object_path objPath)
        -> sdbusplus::async::task<>;

    /* @brief The handler for the image processing failure  */
    void processImageFailed(sdbusplus::message::unix_fd image, std::string& id);

    /** @brief The random generator for the software id */
    std::mt19937 randomGen{static_cast<unsigned>(
        std::chrono::system_clock::now().time_since_epoch().count())};

    /** @brief D-Bus context */
    sdbusplus::async::context& ctx;
    /** @brief item_updater object */
    std::unique_ptr<ItemUpdaterIntf> itemUpdater;
    /** @brief State whether update is in progress */
    bool updateInProgress = false;
};

} // namespace phosphor::software::manager
