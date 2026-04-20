#include "max10_base_cpld.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <array>
#include <string_view>

namespace phosphor::software::cpld
{

namespace
{
constexpr std::array<std::pair<max10Chip, std::string_view>, 4> chipNameMap = {{
    {max10Chip::MAX10_10M04, "MAX10_10M04"},
    {max10Chip::MAX10_10M08, "MAX10_10M08"},
    {max10Chip::MAX10_10M16, "MAX10_10M16"},
    {max10Chip::MAX10_10M25, "MAX10_10M25"},
}};
} // namespace

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
    auto it = std::find_if(
        chipNameMap.begin(), chipNameMap.end(),
        [chip](const auto& pair) { return pair.first == chip; });

    if (it == chipNameMap.end())
    {
        lg2::error("Unsupported chip enum: {CHIPENUM}", "CHIPENUM",
                   static_cast<int>(chip));
        return "";
    }

    std::string moduleName(it->second);
    std::replace(moduleName.begin(), moduleName.end(), '_', '-');
    return moduleName;
}

std::string getMax10ConfigType(max10Chip chip)
{
    auto it = std::find_if(
        chipNameMap.begin(), chipNameMap.end(),
        [chip](const auto& pair) { return pair.first == chip; });

    if (it == chipNameMap.end())
    {
        lg2::error("Unsupported chip enum: {CHIPENUM}", "CHIPENUM",
                   static_cast<int>(chip));
        return "";
    }

    return std::string("Altera") + std::string(it->second) + "Firmware";
}

} // namespace phosphor::software::cpld
