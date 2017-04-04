#include "download_manager.hpp"
#include <iostream>
#include <string>

namespace phosphor
{
namespace software
{
namespace manager
{

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Common::server;

std::string Download::fileName(const std::string value)
{
    // Only call downloadViaTFTP() if both serverAddress and fileName are set
    auto serverAddress = server::TFTP::serverAddress();
    if (serverAddress.empty())
    {
        return server::TFTP::fileName(value);
    }
    downloadViaTFTP(value, serverAddress);

    // Clear fileName and serverAddress
    server::TFTP::serverAddress("");
    return server::TFTP::fileName("");
}

std::string Download::serverAddress(const std::string value)
{
    auto fileName = server::TFTP::fileName();
    if (fileName.empty())
    {
        return server::TFTP::serverAddress(value);
    }
    downloadViaTFTP(fileName, value);

    // Clear fileName and serverAddress
    server::TFTP::fileName("");
    return server::TFTP::serverAddress("");
}

void Download::downloadViaTFTP(const std::string& fileName,
                               const std::string& serverAddress)
{
    return;
}


} // namespace manager
} // namespace software
} // namespace phosphor

