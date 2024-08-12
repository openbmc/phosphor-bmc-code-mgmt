#pragma once

#include "xyz/openbmc_project/Common/FilePath/server.hpp"
#include "xyz/openbmc_project/Inventory/Decorator/Compatible/server.hpp"
#include "xyz/openbmc_project/Object/Delete/server.hpp"
#include "xyz/openbmc_project/Software/ExtendedVersion/server.hpp"
#include "xyz/openbmc_project/Software/Version/server.hpp"

#include <sdbusplus/bus.hpp>

#include <functional>
#include <string>
#include <vector>

namespace phosphor
{
namespace software
{
namespace manager
{

typedef std::function<void(std::string)> eraseFunc;

using VersionInherit = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::software::ExtendedVersion,
    sdbusplus::server::xyz::openbmc_project::software::Version,
    sdbusplus::server::xyz::openbmc_project::common::FilePath,
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::Compatible>;
using DeleteInherit = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::object::Delete>;

class Version;
class Delete;

/** @class Delete
 *  @brief OpenBMC Delete implementation.
 *  @details A concrete implementation for xyz.openbmc_project.Object.Delete
 *  D-Bus API.
 */
class Delete : public DeleteInherit
{
  public:
    /** @brief Constructs Delete.
     *
     *  @param[in] bus    - The D-Bus bus object
     *  @param[in] path   - The D-Bus object path
     *  @param[in] parent - Parent object.
     */
    Delete(sdbusplus::bus_t& bus, const std::string& path, Version& parent) :
        DeleteInherit(bus, path.c_str(), action::emit_interface_added),
        parent(parent)
    {
        // Empty
    }

    /** @brief delete the D-Bus object. */
    void delete_() override;

  private:
    /** @brief Parent Object. */
    Version& parent;
};

/** @class Version
 *  @brief OpenBMC version software management implementation.
 *  @details A concrete implementation for xyz.openbmc_project.Software.Version
 *  D-Bus API.
 */
class Version : public VersionInherit
{
  public:
    /** @brief Constructs Version Software Manager
     *
     * @param[in] bus             - The D-Bus bus object
     * @param[in] objPath         - The D-Bus object path
     * @param[in] versionString   - The version string
     * @param[in] versionPurpose  - The version purpose
     * @param[in] extVersion      - The extended version
     * @param[in] filePath        - The image filesystem path
     * @param[in] compatibleNames - The device compatibility names
     * @param[in] callback        - The eraseFunc callback
     */
    Version(sdbusplus::bus_t& bus, const std::string& objPath,
            const std::string& versionString, VersionPurpose versionPurpose,
            const std::string& extVersion, const std::string& filePath,
            const std::vector<std::string>& compatibleNames, eraseFunc callback,
            const std::string& id) :
        VersionInherit(bus, (objPath).c_str(),
                       VersionInherit::action::defer_emit),
        eraseCallback(std::move(callback)), id(id), objPath(objPath),
        versionStr(versionString)
    {
        // Set properties.
        extendedVersion(extVersion);
        purpose(versionPurpose);
        version(versionString);
        path(filePath);
        names(compatibleNames);
        // Emit deferred signal.
        emit_object_added();
    }

    /**
     * @brief Read the manifest file to get the value of the key.
     *
     * @return The value of the key.
     **/
    static std::string getValue(const std::string& manifestFilePath,
                                std::string key);

    /**
     * @brief Read the manifest file to get the values of the repeated key.
     *
     * @return The values of the repeated key.
     **/
    static std::vector<std::string>
        getRepeatedValues(const std::string& manifestFilePath, std::string key);

    /**
     * @brief Calculate the version id from the version string.
     *
     * @details The version id is a unique 8 hexadecimal digit id
     *          calculated from the version string.
     *
     * @param[in] versionWithSalt - The image's version string
     *                              (e.g. v1.99.10-19) plus an optional salt
     *                              string.
     *
     * @return The id.
     */
    static std::string getId(const std::string& versionWithSalt);

    /**
     * @brief Get the active BMC machine name string.
     *
     * @param[in] releaseFilePath - The path to the file which contains
     *                              the release machine string.
     *
     * @return The machine name string (e.g. romulus, tiogapass).
     */
    static std::string getBMCMachine(const std::string& releaseFilePath);

    /**
     * @brief Get the BMC Extended Version string.
     *
     * @param[in] releaseFilePath - The path to the file which contains
     *                              the release machine string.
     *
     * @return The extended version string.
     */
    static std::string
        getBMCExtendedVersion(const std::string& releaseFilePath);

    /**
     * @brief Get the active BMC version string.
     *
     * @param[in] releaseFilePath - The path to the file which contains
     *                              the release version string.
     *
     * @return The version string (e.g. v1.99.10-19).
     */
    static std::string getBMCVersion(const std::string& releaseFilePath);

    /* @brief Check if this version is functional.
     *
     * @return - Returns the functional value.
     */
    bool isFunctional() const
    {
        return functional;
    }

    /** @brief Set the functional value.
     * @param[in] value - True or False
     */
    void setFunctional(bool value)
    {
        functional = value;
    }

    /** @brief Persistent Delete D-Bus object */
    std::unique_ptr<Delete> deleteObject;

    /** @brief The parent's erase callback. */
    eraseFunc eraseCallback;

    /** @brief The version ID of the object */
    const std::string id;

    /** @brief The path of the object */
    std::string objPath;

  private:
    /** @brief This Version's version string */
    const std::string versionStr;

    /** @brief If this version is the functional one */
    bool functional = false;
};

} // namespace manager
} // namespace software
} // namespace phosphor
