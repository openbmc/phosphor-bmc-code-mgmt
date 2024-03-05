#pragma once
#include "config.h"

#include "version.hpp"

#include <sdbusplus/server.hpp>

#ifdef NEW_CODE_UPDATE
#include <xyz/openbmc_project/Software/Update/server.hpp>
#endif // NEW_CODE_UPDATE

#include <chrono>
#include <random>
#include <string>

namespace phosphor::software::manager
{

#ifdef NEW_CODE_UPDATE

using UpdateIntf = sdbusplus::xyz::openbmc_project::Software::server::Update;
using ManagerIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Update>;

/** @class Manager
 *  @brief Contains a map of Version dbus objects.
 *  @details The software image manager class that contains the Version dbus
 *           objects and their version ids.
 */

class Manager : public ManagerIntf
{
  public:
    /** @brief Constructs Manager Class
     *
     * @param[in] bus - The Dbus bus object
     */
    explicit Manager(sdbusplus::bus_t& bus) :
        ManagerIntf(bus, (std::string(SOFTWARE_OBJPATH) + "/bmc").c_str(),
                    action::defer_emit),
        bus(bus)
    {
        createBMCVersion();
    }

    /** @brief Implementation for StartUpdate
     *  Start a firware update to be performed asynchronously.
     *
     *  @param[in] image - This property indicates the file descriptor of the
     * firmware image.
     *  @param[in] applyTime - This property indicates when the software image
     * update should be applied.
     *  @param[in] forceUpdate - This property indicates whether to bypass
     * update policies when applying the provided image.
     *
     *  @return versionObjectPath[sdbusplus::message::object_path] - The object
     * path where the Version object is hosted.
     */
    sdbusplus::message::object_path
        startUpdate(sdbusplus::message::unix_fd image,
                    UpdateIntf::ApplyTimes applyTime,
                    [[maybe_unused]] bool forceUpdate) override;

    void applyStagedImage() override;
#else

/** @class Manager
 *  @brief Contains a map of Version dbus objects.
 *  @details The software image manager class that contains the Version dbus
 *           objects and their version ids.
 */
class Manager
{
  public:
    /** @brief Constructs Manager Class
     *
     * @param[in] bus - The Dbus bus object
     */
    explicit Manager(sdbusplus::bus_t& bus) : bus(bus){};

    /**
     * @brief Verify and untar the tarball. Verify the manifest file.
     *        Create and populate the version and filepath interfaces.
     *
     * @param[in]  tarballFilePath - Tarball path.
     * @param[out] result          - 0 if successful.
     */
    int processImage(const std::string& tarballFilePath);
#endif // !NEW_CODE_UPDATE

    /**
     * @brief Erase specified entry d-bus object
     *        and deletes the image file.
     *
     * @param[in] entryId - unique identifier of the entry
     */
    void erase(std::string entryId);

  private:
    /** @brief Persistent map of Version dbus objects and their
     * version id */
    std::map<std::string, std::unique_ptr<Version>> versions;

    /** @brief Persistent sdbusplus DBus bus connection. */
    sdbusplus::bus_t& bus;

    /** @brief The random generator to get the version salt */
    std::mt19937 randomGen{static_cast<unsigned>(
        std::chrono::system_clock::now().time_since_epoch().count())};

#ifdef NEW_CODE_UPDATE

    /** @brief Whether the update is in progress */
    bool updateInProgress;

    /** @brief Get the current BMC version path. */
    auto getPath() -> std::string;

    /** @brief Create the version object for current BMC version */
    void createBMCVersion();

#endif // !NEW_CODE_UPDATE

    /**
     * @brief Verify and untar the tarball. Verify the manifest file.
     *        Create and populate the version and filepath interfaces.
     *
     * @param[in]  tarballFilePath - Tarball path.
     * @param[out] objectPath      - Object Path to the new version object.
     * @param[out] result          - 0 if successful.
     */
    int processImageInternal(const std::string& tarballFilePath,
                             std::string& objectPath);

    /**
     * @brief Untar the tarball.
     *
     * @param[in]  tarballFilePath - Tarball path.
     * @param[in]  extractDirPath  - Dir path to extract tarball ball to.
     * @param[out] result          - 0 if successful.
     */
    static int unTar(const std::string& tarballFilePath,
                     const std::string& extractDirPath);
};
} // namespace phosphor::software::manager
