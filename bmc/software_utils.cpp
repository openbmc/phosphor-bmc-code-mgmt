#include "software_utils.hpp"

#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::utils
{

static bool writeToFile(int imageFd, FILE* outStream,
                        std::optional<size_t> maxBytes)
{
    constexpr size_t bSize = 4096;
    ssize_t nRead = 0;
    unsigned char buf[bSize];
    size_t totalWritten = 0;

    while (true)
    {
        size_t toRead = bSize;
        if (maxBytes.has_value() && (totalWritten + toRead) > *maxBytes)
        {
            toRead = *maxBytes - totalWritten;
        }

        if (toRead == 0)
        {
            break; // Reached maxBytes limit
        }

        nRead = read(imageFd, buf, toRead);
        if (nRead == 0)
        {
            break; // EOF
        }
        if (nRead < 0)
        {
            error("Failed to read from input file: {ERRNO}", "ERRNO", errno);
            return false;
        }

        if (fwrite(buf, 1, static_cast<size_t>(nRead), outStream) !=
            static_cast<size_t>(nRead))
        {
            error("Failed to write to file");
            return false;
        }

        totalWritten += static_cast<size_t>(nRead);
    }

    return true;
}

bool unTar(int imageFd, const std::string& extractDirPath,
           std::optional<size_t> maxBytes)
{
    std::string tarCmd = "tar -xf - -C " + extractDirPath + " --no-same-owner";
    info("Executing command: {CMD}", "CMD", tarCmd);
    FILE* outStream = popen(tarCmd.c_str(), "w");
    if (outStream == nullptr)
    {
        error("Failed to open pipe to execute command: {CMD}", "CMD", tarCmd);
        return false;
    }

    if (!writeToFile(imageFd, outStream, maxBytes))
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
