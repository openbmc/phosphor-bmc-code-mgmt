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

std::string getMax10ChipName(max10Chip chip, max10Type type)
{
    static const std::unordered_map<max10Chip, std::string> chipNameMap = {
        {max10Chip::MAX10_10M04, "MAX10_10M04"},
        {max10Chip::MAX10_10M08, "MAX10_10M08"},
        {max10Chip::MAX10_10M16, "MAX10_10M16"},
        {max10Chip::MAX10_10M25, "MAX10_10M25"},
    };

    if (chipNameMap.find(chip) == chipNameMap.end())
    {
        lg2::error("Unsupported chip enum: {CHIPENUM}", "CHIPENUM", static_cast<int>(chip));
        return "";
    }

    auto chipName = chipNameMap.at(chip);

    switch (type)
    {
        case max10Type::type:
            return std::string("Altera") + chipName + "Firmware";
        case max10Type::model:
            std::replace(chipName.begin(), chipName.end(), '_', '-');
            return chipName;
        default:
            lg2::error("Unsupported type: {TYPE}", "TYPE", static_cast<int>(type));
            return "";
    }
}

} // namespace phosphor::software::cpld
