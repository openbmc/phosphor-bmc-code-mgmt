#include "software_utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cstring>

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
        size_t magicLen;
        unsigned char magic[6];
        const char* tarOption;
    };

    static constexpr TarFormat tarFormats[] = {
        // gzip:     1F 8B
        {2, {0x1f, 0x8b}, "-xzf"},
        // bzip2:    42 5A 68 ('BZh')
        {3, {0x42, 0x5a, 0x68}, "-xjf"},
        // xz:       FD 37 7A 58 5A 00
        {6, {0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00}, "-xJf"},
        // compress: 1F 9D
        {2, {0x1f, 0x9d}, "-xZf"},
    };

    unsigned char magic[6] = {0};
    ssize_t bytesRead = pread(imageFd, magic, sizeof(magic), 0);

    for (const auto& fmt : tarFormats)
    {
        if (bytesRead >= static_cast<ssize_t>(fmt.magicLen) &&
            std::memcmp(magic, fmt.magic, fmt.magicLen) == 0)
        {
            return fmt.tarOption;
        }
    }

    return "-xf"; // plain tar
}

bool unTar(int imageFd, const std::string& extractDirPath)
{
    std::string tarCmd = std::string("tar ") + detectTarOption(imageFd) +
                         " - -C " + extractDirPath + " --no-same-owner";

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
