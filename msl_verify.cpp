#include "msl_verify.hpp"
#include "config.h"
#include "version.hpp"

#include <cstring>
#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <phosphor-logging/log.hpp>
#include <regex>
#include <vector>

namespace fs = std::experimental::filesystem;
namespace variant_ns = sdbusplus::message::variant_ns;
using namespace phosphor::logging;
using AssociationList =
    std::vector<std::tuple<std::string, std::string, std::string>>;

int minimum_ship_level::compare(const Version &StrToCompare,
                                const Version &MslString) {
    if (StrToCompare.major > MslString.major)
        return (1);
    else if (StrToCompare.major < MslString.major)
        return (-1);

    if (StrToCompare.minor > MslString.minor)
        return (1);
    else if (StrToCompare.minor < MslString.minor)
        return (-1);

    if (StrToCompare.rev > MslString.rev)
        return (1);
    else if (StrToCompare.rev < MslString.rev)
        return (-1);

    // Both string are equal and ther is no need to make an upgrade return 0.
    return 0;
}

// parse Function copy Input String onto OutString in Version format
// {major,minor,ver}.
void minimum_ship_level::parse(const std::string &InpString,
                               Version &OutString) {
    std::smatch match;
    OutString = {0, 0, 0};

    std::regex rx{REGEX_BMC_MSL, std::regex::extended};

    if (!std::regex_search(InpString, match, rx)) {
        log<level::ERR>("Unable to parse BMC version",
                        entry("VERSION=%s", InpString.c_str()));
        return;
    }

    for (std::size_t index = 0; index < match.size(); ++index) {
        std::cout << match[index] << '\n';
    }

    OutString.major = std::stoi(match[2]);
    OutString.minor = std::stoi(match[3]);
    OutString.rev = std::stoi(match[4]);
}

int minimum_ship_level::verify(const std::string &InpVersionStr) {
    uint InpStrFlag = 0;

    //  If there is no msl or msl string return upgrade is needed.
    std::string msl{BMC_MSL};
    std::string mslRegex{REGEX_BMC_MSL};
    if (msl.empty() || mslRegex.empty()) {
        return true;
    }

    // Define MslVersion variable and populate in Version format
    // {major,minor,ver} using parse function.
    Version MslVersion = {0, 0, 0};
    parse(msl, MslVersion);

    // Define InpTestString variable and populate in Version format
    // {major,minor,version} using parse function.
    Version InpTestString = {0, 0, 0};
    parse(InpVersionStr, InpTestString);

    // Compare Input test string vs MSL.
    InpStrFlag = compare(InpTestString, MslVersion);

    return InpStrFlag;
}
