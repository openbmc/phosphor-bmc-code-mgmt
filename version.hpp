#pragma once

#include "xyz/openbmc_project/Common/FilePath/server.hpp"
#include "xyz/openbmc_project/Object/Delete/server.hpp"
#include "xyz/openbmc_project/Software/Version/server.hpp"

#include <functional>
#include <sdbusplus/bus.hpp>

namespace phosphor
{
namespace software
{
namespace manager
{

typedef std::function<void(std::string)> eraseFunc;

using VersionInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Version,
    sdbusplus::xyz::openbmc_project::Common::server::FilePath>;
using DeleteInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Object::server::Delete>;

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
    Delete(sdbusplus::bus::bus& bus, const std::string& path, Version& parent) :
        DeleteInherit(bus, path.c_str(), true), parent(parent), bus(bus),
        path(path)
    {
        std::vector<std::string> interfaces({interface});
        bus.emit_interfaces_added(path.c_str(), interfaces);
    }

    ~Delete()
    {
        std::vector<std::string> interfaces({interface});
        bus.emit_interfaces_removed(path.c_str(), interfaces);
    }

    /** @brief delete the D-Bus object. */
    void delete_() override;

  private:
    /** @brief Parent Object. */
    Version& parent;

    // TODO Remove once openbmc/openbmc#1975 is resolved
    static constexpr auto interface = "xyz.openbmc_project.Object.Delete";
    sdbusplus::bus::bus& bus;
    std::string path;
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
     * @param[in] bus            - The D-Bus bus object
     * @param[in] objPath        - The D-Bus object path
     * @param[in] versionString  - The version string
     * @param[in] versionPurpose - The version purpose
     * @param[in] filePath       - The image filesystem path
     * @param[in] callback       - The eraseFunc callback
     */
    Version(sdbusplus::bus::bus& bus, const std::string& objPath,
            const std::string& versionString, VersionPurpose versionPurpose,
            const std::string& filePath, eraseFunc callback) :
        VersionInherit(bus, (objPath).c_str(), true),
        versionStr(versionString)
    {
        // Bind erase method
        eraseCallback = callback;
        // Set properties.
        purpose(versionPurpose);
        version(versionString);
        path(filePath);
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
     * @brief Calculate the version id from the version string.
     *
     * @details The version id is a unique 8 hexadecimal digit id
     *          calculated from the version string.
     *
     * @param[in] version - The image's version string (e.g. v1.99.10-19).
     *
     * @return The id.
     */
    static std::string getId(const std::string& version);

    /**
     * @brief Get the active BMC version string.
     *
     * @param[in] releaseFilePath - The path to the file which contains
     *                              the release version string.
     *
     * @return The version string (e.g. v1.99.10-19).
     */
    static std::string getBMCVersion(const std::string& releaseFilePath);

    /* @brief Check if this version matches the currently running version
     *
     * @return - Returns true if this version matches the currently running
     *           version.
     */
    bool isFunctional();

    /** @brief Persistent Delete D-Bus object */
    std::unique_ptr<Delete> deleteObject;

    /** @brief The parent's erase callback. */
    eraseFunc eraseCallback;

  private:
    /** @brief This Version's version string */
    const std::string versionStr;
};

} // namespace manager
} // namespace software
} // namespace phosphor
