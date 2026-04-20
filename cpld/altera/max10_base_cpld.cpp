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

std::string getMax10ModuleName(max10Chip chip)
{
    static const std::unordered_map<max10Chip, std::string> chipNameMap = {
        {max10Chip::MAX10_10M04, "MAX10_10M04"},
        {max10Chip::MAX10_10M08, "MAX10_10M08"},
        {max10Chip::MAX10_10M16, "MAX10_10M16"},
        {max10Chip::MAX10_10M25, "MAX10_10M25"},
    };

    if (chipNameMap.find(chip) == chipNameMap.end())
    {
        lg2::error("Unsupported chip enum: {CHIPENUM}", "CHIPENUM",
                   static_cast<int>(chip));
        return "";
    }

    auto moduleName = chipNameMap.at(chip);
    std::replace(moduleName.begin(), moduleName.end(), '_', '-');
    return moduleName;
}

std::string getMax10TypeName(max10Chip chip)
{
    static const std::unordered_map<max10Chip, std::string> chipNameMap = {
        {max10Chip::MAX10_10M04, "MAX10_10M04"},
        {max10Chip::MAX10_10M08, "MAX10_10M08"},
        {max10Chip::MAX10_10M16, "MAX10_10M16"},
        {max10Chip::MAX10_10M25, "MAX10_10M25"},
    };

    if (chipNameMap.find(chip) == chipNameMap.end())
    {
        lg2::error("Unsupported chip enum: {CHIPENUM}", "CHIPENUM",
                   static_cast<int>(chip));
        return "";
    }

    return std::string("Altera") + chipNameMap.at(chip) + "Firmware";
}

} // namespace phosphor::software::cpld
