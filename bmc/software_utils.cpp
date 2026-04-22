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

bool isCompressedTar(int imageFd)
{
    unsigned char magic[6] = {0};
    ssize_t bytesRead = read(imageFd, magic, sizeof(magic));
    lseek(imageFd, 0, SEEK_SET);

    if (bytesRead < 2)
    {
        return false;
    }

    // gzip
    if (magic[0] == 0x1f && magic[1] == 0x8b)
    {
        return true;
    }

    // bzip2
    if (bytesRead >= 3 && magic[0] == 0x42 && magic[1] == 0x5a &&
        magic[2] == 0x68)
    {
        return true;
    }

    // xz
    if (bytesRead >= 6 && magic[0] == 0xfd && magic[1] == 0x37 &&
        magic[2] == 0x7a && magic[3] == 0x58 && magic[4] == 0x5a &&
        magic[5] == 0x00)
    {
        return true;
    }

    return false;
}

bool unTar(int imageFd, const std::string& extractDirPath)
{
    std::string tarOptions = isCompressedTar(imageFd) ? "-xzf" : "-xf";
    std::string tarCmd =
        "tar " + tarOptions + " - -C " + extractDirPath + " --no-same-owner";

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
