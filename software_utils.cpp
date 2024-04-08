#include "software_utils.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::utils
{

static bool writeToFile(int imageFd, FILE* outStream)
{
    const int bSize = 100;
    ssize_t nRead = 0;
    unsigned char buf[bSize];

    while ((nRead = read(imageFd, buf, bSize)) > 0)
    {
        if (fwrite(buf, 1, nRead, outStream) != (size_t)nRead)
        {
            error("Failed to write to file");
            return false;
        }
    }
    if (nRead < 0)
    {
        error("Failed to read from input file");
        return false;
    }
    return true;
}

bool unTar(int imageFd, const std::string& extractDirPath)
{
    std::string tarCmd = "tar -xf - -C " + extractDirPath + " --no-same-owner";
    info("Executing command: {CMD}", "CMD", tarCmd);
    FILE* outStream = popen(tarCmd.c_str(), "w");
    if (outStream == nullptr)
    {
        error("Failed to open pipe to execute command: {CMD}", "CMD", tarCmd);
        return false;
    }

    if (!writeToFile(imageFd, outStream))
    {
        error("Failed to write to file");
        pclose(outStream);
        return false;
    }

    if (pclose(outStream) != 0)
    {
        error("Failed to close pipe");
        return false;
    }
    return true;
}

} // namespace phosphor::software::utils

boost::asio::io_context& getIOContext()
{
    static boost::asio::io_context io;
    return io;
}
