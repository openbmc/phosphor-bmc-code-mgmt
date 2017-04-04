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
    downloadViaTFTP(value, server::TFTP::serverAddress());
    return server::TFTP::fileName(value);
}

std::string Download::serverAddress(const std::string value)
{
    downloadViaTFTP(server::TFTP::fileName(), value);
    return server::TFTP::serverAddress(value);
}

void Download::downloadViaTFTP(const std::string& file,
                               const std::string& server)
{
    return;
}


} // namespace manager
} // namespace software
} // namespace phosphor

