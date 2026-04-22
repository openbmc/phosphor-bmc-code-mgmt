#include "software_utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

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

static const char* detectTarOption(int imageFd)
{
    struct TarFormat
    {
        std::vector<uint8_t> magic;
        const char* flag;
    };

    static const TarFormat tarFormats[] = {
        {{0x1f, 0x8b}, "-z"},                         // gzip
        {{0x42, 0x5a, 0x68}, "-j"},                   // bzip2
        {{0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00}, "-J"}, // xz
        {{0x1f, 0x9d}, "-Z"},                         // compress
    };

    size_t maxLen = 0;
    for (const auto& fmt : tarFormats)
    {
        maxLen = std::max(maxLen, fmt.magic.size());
    }

    std::vector<uint8_t> buf(maxLen);
    ssize_t bytesRead = pread(imageFd, buf.data(), buf.size(), 0);

    for (const auto& fmt : tarFormats)
    {
        if (static_cast<size_t>(bytesRead) >= fmt.magic.size() &&
            std::equal(fmt.magic.begin(), fmt.magic.end(), buf.begin()))
        {
            return fmt.flag;
        }
    }

    return "";
}

bool unTar(int imageFd, const std::string& extractDirPath)
{
    std::string tarCmd = "tar -x " + std::string(detectTarOption(imageFd)) +
                         " -f - -C " + extractDirPath + " --no-same-owner";
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
