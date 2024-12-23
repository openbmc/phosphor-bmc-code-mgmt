#include "pt5161l.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>
#include <iomanip>
#include <sstream>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software::eeprom::version;

std::string PT5161LVersionProvider::getVersion()
{
    std::string version;
    std::ostringstream busOss;
    std::ostringstream addrOss;

    busOss << std::setw(2) << std::setfill('0') << static_cast<int>(bus);
    addrOss << std::setw(4) << std::setfill('0') << std::hex << std::nouppercase
            << static_cast<int>(address);

    // The PT5161L driver exposes the firmware version through the fw_ver node
    std::string path = "/sys/kernel/debug/" + chipModel + "/" + busOss.str() +
                       "-" + addrOss.str() + "/fw_ver";

    std::ifstream file(path);
    if (!file)
    {
        error("Failed to get version: unable to open file: {PATH}", "PATH",
              path);
        return version;
    }

    if (!std::getline(file, version) || version.empty())
    {
        error("Failed to read version from file: {PATH}", "PATH", path);
    }

    return version;
}

bool PT5161LVersionProvider::isHostOnRequiredToGetVersion()
{
    return true;
}
