#include "config.h"

#include "msl_verify.hpp"

#include "version.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Software/Version/error.hpp>

#include <regex>

PHOSPHOR_LOG2_USING;

int minimum_ship_level::compare(const Version& versionToCompare,
                                const Version& mslVersion)
{
    if (versionToCompare.major > mslVersion.major)
        return (1);
    if (versionToCompare.major < mslVersion.major)
        return (-1);

    if (versionToCompare.minor > mslVersion.minor)
        return (1);
    if (versionToCompare.minor < mslVersion.minor)
        return (-1);

    if (versionToCompare.rev > mslVersion.rev)
        return (1);
    if (versionToCompare.rev < mslVersion.rev)
        return (-1);

    // Both string are equal and there is no need to make an upgrade return 0.
    return 0;
}

// parse Function copy  inpVersion onto outVersion in Version format
// {major,minor,rev}.
void minimum_ship_level::parse(const std::string& inpVersion,
                               Version& outVersion)
{
    std::smatch match;
    outVersion = {0, 0, 0};

    std::regex rx{REGEX_BMC_MSL, std::regex::extended};

    if (!std::regex_search(inpVersion, match, rx))
    {
        error("Unable to parse BMC version: {VERSION}", "VERSION", inpVersion);
        return;
    }

    outVersion.major = std::stoi(match[2]);
    outVersion.minor = std::stoi(match[3]);
    outVersion.rev = std::stoi(match[4]);
}

bool minimum_ship_level::verify(const std::string& versionManifest)
{
    //  If there is no msl or mslRegex return upgrade is needed.
    std::string msl{BMC_MSL};
    std::string mslRegex{REGEX_BMC_MSL};
    if (msl.empty() || mslRegex.empty())
    {
        return true;
    }

    // Define mslVersion variable and populate in Version format
    // {major,minor,rev} using parse function.

    Version mslVersion = {0, 0, 0};
    parse(msl, mslVersion);

    // Define actualVersion variable and populate in Version format
    // {major,minor,rev} using parse function.
    std::string tmpStr{};

    tmpStr = versionManifest;
    Version actualVersion = {0, 0, 0};
    parse(versionManifest, actualVersion);

    // Compare actualVersion vs MSL.
    auto rc = compare(actualVersion, mslVersion);
    if (rc < 0)
    {
        using namespace phosphor::logging;
        using IncompatibleErr = sdbusplus::xyz::openbmc_project::Software::
            Version::Error::Incompatible;
        using Incompatible =
            xyz::openbmc_project::Software::Version::Incompatible;
        std::string purpose =
            "xyz.openbmc_project.Software.Version.VersionPurpose.BMC";

        error(
            "BMC Minimum Ship Level ({MIN_VERSION}) NOT met by {ACTUAL_VERSION}",
            "MIN_VERSION", msl, "ACTUAL_VERSION", tmpStr, "VERSION_PURPOSE",
            purpose);
        report<IncompatibleErr>(Incompatible::MIN_VERSION(msl.c_str()),
                                Incompatible::ACTUAL_VERSION(tmpStr.c_str()),
                                Incompatible::VERSION_PURPOSE(purpose.c_str()));
        return false;
    }

    return true;
}
