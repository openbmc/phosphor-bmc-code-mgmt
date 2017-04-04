#pragma once

#include <sdbusplus/bus.hpp>
#include "xyz/openbmc_project/Common/TFTP/server.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using DownloadInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Common::server::TFTP>;

/** @class Download
 *  @brief OpenBMC download software management implementation.
 *  @details A concrete implementation for xyz.openbmc_project.Common.TFTP
 *  DBus API.
 */
class Download : public DownloadInherit
{
    public:
        /** @brief Constructs Download Software Manager
         *
         * @param[in] bus       - The Dbus bus object
         * @param[in] objPath   - The Dbus object path
         */
        Download(sdbusplus::bus::bus& bus,
                const std::string& objPath) : DownloadInherit(
                    bus, (objPath).c_str()) {};

        /** @brief Set value of FileName*/
        std::string fileName(const std::string value) override;

        /** @brief Set value of ServerAddress */
        std::string serverAddress(const std::string value) override;

    private:
        /**
         * @brief Download the specified image via TFTP
         *
         * @param[in] file   - The name of the file to transfer.
         * @param[in] server - The TFTP Server IP Address.
         **/
        void downloadViaTFTP(const std::string& file,
                             const std::string& server);

        /** @brief The name of the file to transfer. */
        std::string file;

        /** @brief The TFTP Server IP Address. */
        std::string server;
};

} // namespace manager
} // namespace software
} // namespace phosphor

