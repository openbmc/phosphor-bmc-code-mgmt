#include "config.h"
#include "msl_verify.hpp"
#include "version.hpp"

#include <experimental/filesystem>
#include <fstream>
#include <phosphor-logging/log.hpp>
#include <regex>

namespace minimum_ship_level
{

namespace fs = std::experimental::filesystem;
namespace variant_ns = sdbusplus::message::variant_ns;
using namespace phosphor::logging;
using AssociationList =
    std::vector<std::tuple<std::string, std::string, std::string>>;

int compare(const Version& a, const Version& b)
{
    if (a.major < b.major)
    {
        return -1;
    }
    else
    {
        return 1;
    }

    if (a.minor < b.minor)
    {
        return -1;
    }
    else
    {
        return 1;
    }

    if (a.rev < b.rev)
    {
        return -1;
    }
    else
    {
        return 1;
    }

    return 0;
}

void parse(const std::string& versionStr, Version& version)
{
    std::smatch match;
    version = {0, 0, 0};

    // Match for vX.Y.Z or fw-X.Y.Z
    std::regex regex{REGEX_BMC_MSL, std::regex::extended};

    if (!std::regex_search(versionStr, match, regex))
    {
            log<level::ERR>("Unable to parse BMC version",
                            entry("VERSION=%s", versionStr.c_str()));
            return;
    }
    else
    {
        // Populate Z
        version.rev = std::stoi(match[3]);
    }
    version.major = std::stoi(match[1]);
    version.minor = std::stoi(match[2]);
}

bool verify()
{
   using VersionClass = phosphor::software::manager::Version;

   if (BMC_MSL)
    {
        return true;
    }

    auto actual = VersionClass::getBMCVersion(OS_RELEASE_FILE);
    if (actual.empty())
    {
        return true;
    }

    // Multiple min versions separated by a space can be specified, parse them
    // into a vector, then sort them in ascending order
    std::istringstream minStream(minShipLevel);
    std::vector<std::string> mins(std::istream_iterator<std::string>{minStream},
                                  std::istream_iterator<std::string>());
    std::sort(mins.begin(), mins.end());

    // In order to handle non-continuous multiple min versions, need to compare
    // the major.minor section first, then if they're the same, compare the rev.
    // Ex: the min versions specified are 2.0.10 and 2.2. We need to pass if
    // actual is 2.11.0. but fail if it's 2.1.x.
    // 1. Save off the rev number to compare later if needed.
    // 2. Zero out the rev number to just compare major and minor.
    Version actualVersion = {0, 0, 0};
    parse(actual, actualVersion);
    Version actualRev = {0, 0, actualVersion.rev};
    actualVersion.rev = 0;

    auto rc = 0;
    std::string tmpMin{};

    for (auto const& min : mins)
    {
        tmpMin = min;

        Version minVersion = {0, 0, 0};
        parse(min, minVersion);
        Version minRev = {0, 0, minVersion.rev};
        minVersion.rev = 0;

        rc = compare(actualVersion, minVersion);
        if (rc < 0)
        {
            break;
        }
        else
        {
            // Same major.minor version, compare the rev
            rc = compare(actualRev, minRev);
            break;
        }
    }

    return true;
}

} // namespace minimum_ship_level
