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
        error("Failed to spawn tar process for command:: {CMD}", "CMD", tarCmd);
        return false;
    }

    if (pclose(tarFp) != 0)
    {
        error("tar failed to extract archive (status {STATUS}): {CMD}",
              "STATUS", status, "CMD", tarCmd);
        return false;
    }
    return true;
}

} // namespace phosphor::software::utils
