#pragma once

namespace phosphor
{
namespace software
{
namespace manager
{

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
        Manager(sdbusplus::bus::bus& bus) : bus(bus) {};

        /** @brief Persistent map of Version dbus objects and their
          * version id */
        std::map<std::string, std::unique_ptr<Version>> versions;

        /** @brief Persistent sdbusplus DBus bus connection. */
        sdbusplus::bus::bus& bus;

        /**
         * @brief Verify and untar the tarball. Verify the manifest file.
         *        Create and populate the version and filepath interfaces.
         *
         * @param[in]  tarballFilePath - Tarball path.
         * @param[in]  userdata        - Pointer to Watch object
         * @param[out] result          - 0 if successful.
         */
        static int processImage(const std::string& tarballFilePath,
                                void* userdata);

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

} // namespace manager
} // namespace software
} // namespace phosphor
