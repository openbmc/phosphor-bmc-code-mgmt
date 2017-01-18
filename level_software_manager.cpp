#include <iostream>
#include <string>
#include <fstream>
#include "level_software_manager.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

std::string Level::getVersion()
{
    // Get version from /etc/os-release.
    std::string version;
    std::ifstream efile;
    std::string line;
    efile.open("/etc/os-release");

    while(!efile.eof()) {
        getline(efile, line);
        if (line.find("VERSION_ID=") != std::string::npos) {
            version = line.substr(12,26);
            break;
        }
    }
    efile.close();
    return version;

}

} // namespace manager
} // namespace software
} // namepsace phosphor
