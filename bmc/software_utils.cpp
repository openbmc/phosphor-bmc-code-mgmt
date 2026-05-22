#include "software_utils.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::utils
{

bool unTar(int imageFd, const std::string& extractDirPath)
{
    std::string tarCmd = "tar -xf /proc/self/fd/" + std::to_string(imageFd) +
                         " -C " + extractDirPath + " --no-same-owner";
    info("Executing command: {CMD}", "CMD", tarCmd);

    FILE* tarFp = popen(tarCmd.c_str(), "r");
    if (tarFp == nullptr)
    {
        error("Failed to open pipe to execute command: {CMD}", "CMD", tarCmd);
        return false;
    }

    if (pclose(tarFp) != 0)
    {
        error("Failed to close pipe");
        return false;
    }
    return true;
}

} // namespace phosphor::software::utils
