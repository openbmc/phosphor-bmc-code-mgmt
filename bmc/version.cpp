#include "config.h"

#include "version.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <openssl/evp.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/lg2.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace phosphor
{
namespace software
{
namespace manager
{

PHOSPHOR_LOG2_USING;
using namespace phosphor::logging;
using Argument = xyz::openbmc_project::common::InvalidArgument;
using namespace sdbusplus::error::xyz::openbmc_project::common;

std::string Version::getValue(const std::string& manifestFilePath,
                              std::string key)
{
    std::vector<std::string> values = getRepeatedValues(manifestFilePath, key);
    if (values.empty())
    {
        return std::string{};
    }
    if (values.size() > 1)
    {
        error("Multiple values found in MANIFEST file for key: {KEY}", "KEY",
              key);
    }
    return values.at(0);
}

std::vector<std::string> Version::getRepeatedValues(
    const std::string& manifestFilePath, std::string key)
{
    key = key + "=";
    auto keySize = key.length();

    if (manifestFilePath.empty())
    {
        error("ManifestFilePath is empty.");
        elog<InvalidArgument>(
            Argument::ARGUMENT_NAME("manifestFilePath"),
            Argument::ARGUMENT_VALUE(manifestFilePath.c_str()));
    }

    std::vector<std::string> values{};
    std::ifstream efile;
    std::string line;
    efile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    // Too many GCC bugs (53984, 66145) to do this the right way...
    try
    {
        efile.open(manifestFilePath);
        while (getline(efile, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                // If the manifest has CRLF line terminators, e.g. is created on
                // Windows, the line will contain \r at the end, remove it.
                line.pop_back();
            }
            if (line.compare(0, keySize, key) == 0)
            {
                values.push_back(line.substr(keySize));
            }
        }
        efile.close();
    }
    catch (const std::exception& e)
    {
        if (!efile.eof())
        {
            error("Error occurred when reading MANIFEST file: {ERROR}", "KEY",
                  key, "ERROR", e);
        }
    }

    if (values.empty())
    {
        info("No values found in MANIFEST file for key: {KEY}", "KEY", key);
    }

    return values;
}

using EVP_MD_CTX_Ptr =
    std::unique_ptr<EVP_MD_CTX, decltype(&::EVP_MD_CTX_free)>;

std::string Version::getId(const std::string& version)
{
    if (version.empty())
    {
        error("Version is empty.");
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("Version"),
                              Argument::ARGUMENT_VALUE(version.c_str()));
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    EVP_MD_CTX_Ptr ctx(EVP_MD_CTX_new(), &::EVP_MD_CTX_free);

    EVP_DigestInit(ctx.get(), EVP_sha512());
    EVP_DigestUpdate(ctx.get(), version.c_str(), strlen(version.c_str()));
    EVP_DigestFinal(ctx.get(), digest.data(), nullptr);

    // We are only using the first 8 characters.
    char mdString[9];
    snprintf(mdString, sizeof(mdString), "%02x%02x%02x%02x",
             (unsigned int)digest[0], (unsigned int)digest[1],
             (unsigned int)digest[2], (unsigned int)digest[3]);

    return mdString;
}

std::string Version::getBMCMachine(const std::string& releaseFilePath)
{
    std::string machineKey = "OPENBMC_TARGET_MACHINE=";
    std::string machine{};
    std::ifstream efile(releaseFilePath);
    std::string line;

    while (getline(efile, line))
    {
        if (line.substr(0, machineKey.size()).find(machineKey) !=
            std::string::npos)
        {
            std::size_t pos = line.find_first_of('"') + 1;
            machine = line.substr(pos, line.find_last_of('"') - pos);
            break;
        }
    }

    if (machine.empty())
    {
        error("Unable to find OPENBMC_TARGET_MACHINE");
        elog<InternalFailure>();
    }

    return machine;
}

std::string Version::getBMCExtendedVersion(const std::string& releaseFilePath)
{
    std::string extendedVersionKey = "EXTENDED_VERSION=";
    std::string extendedVersionValue{};
    std::string extendedVersion{};
    std::ifstream efile(releaseFilePath);
    std::string line;

    while (getline(efile, line))
    {
        if (line.substr(0, extendedVersionKey.size())
                .find(extendedVersionKey) != std::string::npos)
        {
            extendedVersionValue = line.substr(extendedVersionKey.size());
            std::size_t pos = extendedVersionValue.find_first_of('"') + 1;
            extendedVersion = extendedVersionValue.substr(
                pos, extendedVersionValue.find_last_of('"') - pos);
            break;
        }
    }

    return extendedVersion;
}

std::string Version::getBMCVersion(const std::string& releaseFilePath)
{
    std::string versionKey = "VERSION_ID=";
    std::string versionValue{};
    std::string version{};
    std::ifstream efile;
    std::string line;
    efile.open(releaseFilePath);

    while (getline(efile, line))
    {
        if (line.substr(0, versionKey.size()).find(versionKey) !=
            std::string::npos)
        {
            // Support quoted and unquoted values
            // 1. Remove the versionKey so that we process the value only.
            versionValue = line.substr(versionKey.size());

            // 2. Look for a starting quote, then increment the position by 1 to
            //    skip the quote character. If no quote is found,
            //    find_first_of() returns npos (-1), which by adding +1 sets pos
            //    to 0 (beginning of unquoted string).
            std::size_t pos = versionValue.find_first_of('"') + 1;

            // 3. Look for ending quote, then decrease the position by pos to
            //    get the size of the string up to before the ending quote. If
            //    no quote is found, find_last_of() returns npos (-1), and pos
            //    is 0 for the unquoted case, so substr() is called with a len
            //    parameter of npos (-1) which according to the documentation
            //    indicates to use all characters until the end of the string.
            version =
                versionValue.substr(pos, versionValue.find_last_of('"') - pos);
            break;
        }
    }
    efile.close();

    if (version.empty())
    {
        error("BMC current version is empty");
        elog<InternalFailure>();
    }

    return version;
}

void Delete::delete_()
{
    if (parent.eraseCallback)
    {
        parent.eraseCallback(parent.id);
    }
}

} // namespace manager
} // namespace software
} // namespace phosphor
