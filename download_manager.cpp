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
    file = value;
    downloadViaTFTP(value, server);
    return server::TFTP::fileName(value);
}

std::string Download::serverAddress(const std::string value)
{
    server = value;
    downloadViaTFTP(file, value);
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

