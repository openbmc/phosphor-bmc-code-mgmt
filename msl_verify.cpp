#include "config.h"

#include "msl_verify.hpp"

#include "version.hpp"

#include <phosphor-logging/log.hpp>

#include <regex>

using namespace phosphor::logging;

int minimum_ship_level::compare(const Version& strToCompare,
                                const Version& mslString)
{
    if (strToCompare.major > mslString.major)
        return (1);
    if (strToCompare.major < mslString.major)
        return (-1);

    if (strToCompare.minor > mslString.minor)
        return (1);
    if (strToCompare.minor < mslString.minor)
        return (-1);

    if (strToCompare.rev > mslString.rev)
        return (1);
    if (strToCompare.rev < mslString.rev)
        return (-1);

    // Both string are equal and there is no need to make an upgrade return 0.
    return 0;
}

// parse Function copy  inpString onto outString in Version format
// {major,minor,rev}.
void minimum_ship_level::parse(const std::string& inpString, Version& outString)
{
    std::smatch match;
    outString = {0, 0, 0};

    std::regex rx{REGEX_BMC_MSL, std::regex::extended};

    if (!std::regex_search(inpString, match, rx))
    {
        log<level::ERR>("Unable to parse BMC version",
                        entry("VERSION=%s", inpString.c_str()));
        return;
    }

    outString.major = std::stoi(match[2]);
    outString.minor = std::stoi(match[3]);
    outString.rev = std::stoi(match[4]);
}

bool minimum_ship_level::verify(const std::string& inpVersionStr)
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

    // Define inpTestString variable and populate in Version format
    // {major,minor,rev} using parse function.
    std::string tmpStr{};

    tmpStr = inpVersionStr;
    Version inpTestString = {0, 0, 0};
    parse(inpVersionStr, inpTestString);

    // Compare inpTestString vs MSL.
    auto rc = compare(inpTestString, mslVersion);
    if (rc < 0)
    {
        log<level::ERR>(
            "BMC Minimum Ship Level NOT met",
            entry("MIN_VERSION=%s", msl.c_str()),
            entry("ACTUAL_VERSION=%s", tmpStr.c_str()),
            entry("VERSION_PURPOSE=%s",
                  "xyz.openbmc_project.Software.Version.VersionPurpose.BMC"));
        return false;
    }

    return true;
}
