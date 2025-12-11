#pragma once

#include "config.h"

#include <sdbusplus/async.hpp>
#include <xyz/openbmc_project/Software/MultipartUpdate/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <cstdint>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <tuple>

namespace phosphor::software::updater
{
class ItemUpdater;
}

namespace phosphor::software::update
{

using ItemUpdaterIntf = phosphor::software::updater::ItemUpdater;

using ApplyTimeIntf =
    sdbusplus::common::xyz::openbmc_project::software::ApplyTime;

// Compile-time configuration for PLDM multipart update support.
// When enabled, UpdateIntf includes MultipartUpdate interface and
// device-specific IANA/compatible values are set from meson config.
#ifdef BMC_MULTIPART_UPDATE
inline constexpr bool bmcMultipartUpdateEnabled = true;
inline constexpr uint32_t bmcVendorIANA = BMC_VENDOR_IANA;
inline constexpr const char* bmcCompatibleHardware = BMC_COMPATIBLE_HARDWARE;
using UpdateIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Update,
    sdbusplus::xyz::openbmc_project::Software::server::MultipartUpdate>;
#else
inline constexpr bool bmcMultipartUpdateEnabled = false;
inline constexpr uint32_t bmcVendorIANA = 0;
inline constexpr const char* bmcCompatibleHardware = "";
using UpdateIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Update>;
#endif

/** @class Manager
 *  @brief Processes the image file from update D-Bus interface.
 *  @details The update manager class handles software updates and manages
 *  software info through version and activation objects. When
 * BMC_MULTIPART_UPDATE is enabled, also handles PLDM firmware packages by
 * extracting the matching component based on compile-time device descriptors.
 */
class Manager : public UpdateIntf
{
  public:
    /** @brief Constructs Manager Class
     *
     * @param[in] ctx - The D-Bus async context
     * @param[in] path - The D-Bus object path
     * @param[in] itemUpdater - Reference to the ItemUpdater
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
     *  Start a firmware update to be performed asynchronously.
     *  Handles both raw TAR files and PLDM packages (when enabled).
     */
    sdbusplus::message::object_path startUpdate(
        sdbusplus::message::unix_fd image,
        ApplyTimeIntf::RequestedApplyTimes applyTime) override;

    // allowedApplyTimes is only needed when MultipartUpdate interface is
    // present
#ifdef BMC_MULTIPART_UPDATE
    /** @brief Get allowed apply times for MultipartUpdate interface */
    std::set<ApplyTimeIntf::RequestedApplyTimes> allowedApplyTimes()
        const override
    {
        return {ApplyTimeIntf::RequestedApplyTimes::Immediate,
                ApplyTimeIntf::RequestedApplyTimes::OnReset};
    }
#endif

    /** @brief Result of PLDM package parsing (always defined for if constexpr)
     */
    enum class PldmParseResult
    {
        NotPldm,    // Not a PLDM package, try as TAR
        PldmMatch,  // PLDM package with matching component found
        PldmNoMatch // PLDM package but no matching device descriptor
    };

    /** @brief Try to parse image as PLDM package
     *  @param[in] fd - File descriptor of the image
     *  @param[out] offset - Component offset if PLDM match
     *  @param[out] size - Component size if PLDM match
     *  @return PldmParseResult indicating parse outcome
     *  @note Returns NotPldm when bmcMultipartUpdateEnabled is false
     */
    static PldmParseResult tryParsePldmPackage(int fd, size_t& offset,
                                               size_t& size);

    /* @brief Process the image supplied via image fd */
    auto processImage(sdbusplus::message::unix_fd image,
                      ApplyTimeIntf::RequestedApplyTimes applyTime,
                      std::string id, std::string objPath,
                      std::optional<size_t> maxBytes = std::nullopt)
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
