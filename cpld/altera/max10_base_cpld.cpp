#include "max10_base_cpld.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <unordered_map>

namespace phosphor::software::cpld
{

const std::map<max10Chip, Max10CpldInfo>& getSupportedDeviceMap()
{
    static const std::map<max10Chip, Max10CpldInfo> supportedDeviceMap = {
        {max10Chip::MAX10_10M04, {max10ChipFamily::MAX10, {}}},
        {max10Chip::MAX10_10M08, {max10ChipFamily::MAX10, {}}},
        {max10Chip::MAX10_10M16, {max10ChipFamily::MAX10, {}}},
        {max10Chip::MAX10_10M25, {max10ChipFamily::MAX10, {}}},
    };
    return supportedDeviceMap;
}

std::string getMax10ChipStr(max10Chip chip, max10StringType stringType)
{
    static const std::unordered_map<max10Chip, std::string> chipStringMap = {
        {max10Chip::MAX10_10M04, "MAX10_10M04"},
        {max10Chip::MAX10_10M08, "MAX10_10M08"},
        {max10Chip::MAX10_10M16, "MAX10_10M16"},
        {max10Chip::MAX10_10M25, "MAX10_10M25"},
    };

    if (chipStringMap.find(chip) == chipStringMap.end())
    {
        lg2::error("Unsupported chip enum: {CHIPENUM}", "CHIPENUM", chip);
        return "";
    }

    auto chipString = chipStringMap.at(chip);

    switch (stringType)
    {
        case max10StringType::typeString:
            return std::string("Altera") + chipString + "Firmware";
        case max10StringType::modelString:
            std::replace(chipString.begin(), chipString.end(), '_', '-');
            return chipString;
        default:
            lg2::error("Unsupported string type: {STRINGTYPE}", "STRINGTYPE",
                       stringType);
            return "";
    }
}

} // namespace phosphor::software::cpld
