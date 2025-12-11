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
// FirmwareInfo (VendorIANA, CompatibleHardware) is discovered at runtime
// from Entity Manager's xyz.openbmc_project.Configuration.BMC interface.
#ifdef BMC_MULTIPART_UPDATE
inline constexpr bool bmcMultipartUpdateEnabled = true;
#else
inline constexpr bool bmcMultipartUpdateEnabled = false;
#endif

// Interface type that includes both Update and MultipartUpdate
using UpdateIntfWithMultipart = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Update,
    sdbusplus::xyz::openbmc_project::Software::server::MultipartUpdate>;

// Interface type that only includes Update
using UpdateIntfOnly = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Update>;

/** @class ManagerImpl
 *  @brief Processes the image file from update D-Bus interface.
 *  @details The update manager class handles software updates and manages
 *  software info through version and activation objects. When
 * BMC_MULTIPART_UPDATE is enabled, also handles PLDM firmware packages by
 * extracting the matching component based on compile-time device descriptors.
 *  @tparam UpdateIntf The D-Bus interface type (UpdateIntfOnly or
 * UpdateIntfWithMultipart)
 */
template <typename UpdateIntf>
class ManagerImpl : public UpdateIntf
{
  public:
    /** @brief Constructs Manager Class
     *
     * @param[in] ctx - The D-Bus async context
     * @param[in] path - The D-Bus object path
     * @param[in] itemUpdater - Reference to the ItemUpdater
     */
    explicit ManagerImpl(sdbusplus::async::context& ctx,
                         const std::string& path,
                         ItemUpdaterIntf& itemUpdater) :
        UpdateIntf(ctx.get_bus(), path.c_str(), UpdateIntf::action::defer_emit),
        ctx(ctx), itemUpdater(itemUpdater)
    {
        this->emit_object_added();
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
     *  @param[in] vendorIANA - Vendor IANA to match
     *  @param[in] compatibleHardware - Compatible hardware string to match
     *  @param[out] offset - Component offset if PLDM match
     *  @param[out] size - Component size if PLDM match
     *  @return PldmParseResult indicating parse outcome
     *  @note Returns NotPldm when bmcMultipartUpdateEnabled is false
     *        or when vendorIANA is 0 (firmware info not configured)
     */
    static PldmParseResult tryParsePldmPackage(
        int fd, std::optional<uint32_t> vendorIANA,
        std::optional<std::string> compatibleHardware, size_t& offset,
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

// Type aliases for different manager configurations:
// - UpdateManager: Only exposes Software.Update (for per-version objects)
// - MultipartUpdateManager: Exposes both Software.Update and
//   Software.MultipartUpdate (for main BMC path when multipart is enabled)
using UpdateManager = ManagerImpl<UpdateIntfOnly>;
using MultipartUpdateManager = ManagerImpl<UpdateIntfWithMultipart>;

} // namespace phosphor::software::update
